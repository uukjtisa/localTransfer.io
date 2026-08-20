// ================================================================
//  localTransfer.io  –  media.cpp
//  ffmpeg-optional thumbnails, poster frames, durations and clips.
//  See media.h for the contract and the cache layout.
// ================================================================
#include "media.h"
#include "utils.h"
#include "database.h"

static std::atomic<bool> g_hasFfmpeg{false};
static std::atomic<bool> g_hasFfprobe{false};
static std::string       g_thumbDir;

// One generation at a time per key. A second request for the same key waits
// rather than launching a duplicate ffmpeg.
static std::mutex                    g_genMtx;
static std::map<std::string, bool>   g_inFlight;

bool mediaHasFfmpeg() { return g_hasFfmpeg.load(); }

// ── process helper ──────────────────────────────────────────────
// Runs a command with no console window and waits. Returns the exit code, or
// -1 if the process could not be started at all.
static int runHidden(const std::wstring& cmdline, DWORD timeoutMs = 60000) {
    std::wstring mutableCmd = cmdline;   // CreateProcessW may modify this
    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return -1;
    DWORD w = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD code = 1;
    if (w == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else                    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return (int)code;
}

// Runs a command and captures stdout. Used only for ffprobe, whose output is
// a single short line.
static std::string runCapture(const std::wstring& cmdline, DWORD timeoutMs = 20000) {
    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return "";
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    std::wstring mutableCmd = cmdline;
    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr); return "";
    }
    CloseHandle(wr);                       // our copy; child holds the other

    std::string out;
    char buf[512];
    DWORD got = 0;
    while (ReadFile(rd, buf, sizeof(buf), &got, nullptr) && got > 0) out.append(buf, got);
    WaitForSingleObject(pi.hProcess, timeoutMs);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    return out;
}

static bool fileExists(const std::string& p) {
    return GetFileAttributesW(toWide(p).c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Quote a path for a command line. ffmpeg paths can contain spaces.
static std::wstring q(const std::string& s) { return L"\"" + toWide(s) + L"\""; }

// ── init ────────────────────────────────────────────────────────
void mediaInit() {
    g_thumbDir = getExeDir() + "\\thumbs";
    CreateDirectoryW(toWide(g_thumbDir).c_str(), nullptr);

    // "-version" exits 0 when the tool is present. Cheap and unambiguous.
    g_hasFfmpeg  = (runHidden(L"ffmpeg -version",  8000) == 0);
    g_hasFfprobe = (runHidden(L"ffprobe -version", 8000) == 0);

    if (g_hasFfmpeg)
        Log(L_OK, std::string("ffmpeg found — thumbnails and previews enabled") +
                  (g_hasFfprobe ? "" : " (ffprobe missing: durations unavailable)"));
    else
        Log(L_INFO, "ffmpeg not on PATH — serving original images as thumbnails, no video previews");
}

// ── keys and paths ──────────────────────────────────────────────
std::string mediaKeyForPath(const std::string& path) {
    // FNV-1a over the lowercased path: stable, short, no filesystem-illegal chars.
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : path) {
        h ^= (unsigned char)tolower(c);
        h *= 1099511628211ULL;
    }
    char buf[24];
    sprintf_s(buf, "ff%016llx", (unsigned long long)h);
    return buf;
}
std::string mediaThumbFile(const std::string& key) { return g_thumbDir + "\\" + key + ".jpg"; }
std::string mediaClipFile (const std::string& key) { return g_thumbDir + "\\" + key + ".mp4"; }
static std::string durFile(const std::string& key) { return g_thumbDir + "\\" + key + ".dur"; }

bool mediaHasThumb(const std::string& key) { return fileExists(mediaThumbFile(key)); }
bool mediaHasClip (const std::string& key) { return fileExists(mediaClipFile(key)); }

uint64_t mediaDuration(const std::string& key) {
    std::ifstream f(durFile(key));
    if (!f.is_open()) return 0;
    uint64_t v = 0; f >> v; return v;
}
static void writeDuration(const std::string& key, uint64_t secs) {
    std::ofstream f(durFile(key), std::ios::trunc);
    if (f.is_open()) f << secs;
}

// ── duration probe ──────────────────────────────────────────────
static uint64_t probeDuration(const std::string& src) {
    if (!g_hasFfprobe) return 0;
    std::wstring cmd = L"ffprobe -v error -show_entries format=duration "
                       L"-of default=noprint_wrappers=1:nokey=1 " + q(src);
    std::string out = trim(runCapture(cmd));
    if (out.empty()) return 0;
    try { double d = std::stod(out); return d > 0 ? (uint64_t)(d + 0.5) : 0; }
    catch (...) { return 0; }
}

// Claim a key for generation; returns false if someone else already holds it
// (in which case we wait for them and use their result).
static bool claim(const std::string& key) {
    for (int spin = 0; spin < 600; ++spin) {          // ~60 s ceiling
        { std::lock_guard<std::mutex> lk(g_genMtx);
          if (!g_inFlight[key]) { g_inFlight[key] = true; return true; } }
        Sleep(100);
    }
    return false;
}
static void release(const std::string& key) {
    std::lock_guard<std::mutex> lk(g_genMtx);
    g_inFlight[key] = false;
}

// ── thumbnails ──────────────────────────────────────────────────
bool mediaEnsureThumb(const std::string& key, const std::string& src, bool isVideo) {
    if (mediaHasThumb(key)) return true;
    if (!g_hasFfmpeg || !fileExists(src)) return false;
    if (!claim(key)) return false;
    if (mediaHasThumb(key)) { release(key); return true; }   // won by someone else

    const std::string out = mediaThumbFile(key);
    int rc = -1;

    if (isVideo) {
        uint64_t dur = mediaDuration(key);
        if (!dur) { dur = probeDuration(src); if (dur) writeDuration(key, dur); }
        // A frame a little way in: frame 0 of a video is very often black.
        double at = dur ? (dur < 12 ? dur * 0.15 : 3.0) : 1.0;
        char ss[32]; sprintf_s(ss, "%.2f", at);
        // -ss before -i seeks by keyframe, which is fast even on large files.
        std::wstring cmd = L"ffmpeg -y -v error -ss " + toWide(ss) + L" -i " + q(src) +
                           L" -frames:v 1 -vf \"scale=480:-2\" -q:v 4 " + q(out);
        rc = runHidden(cmd, 45000);
        if (rc != 0 || !fileExists(out)) {
            // Seeking past the end of a short/odd file: retry from the start.
            std::wstring retry = L"ffmpeg -y -v error -i " + q(src) +
                                 L" -frames:v 1 -vf \"scale=480:-2\" -q:v 4 " + q(out);
            rc = runHidden(retry, 45000);
        }
    } else {
        std::wstring cmd = L"ffmpeg -y -v error -i " + q(src) +
                           L" -frames:v 1 -vf \"scale=480:-2\" -q:v 4 " + q(out);
        rc = runHidden(cmd, 30000);
    }

    bool ok = (rc == 0 && fileExists(out));
    if (!ok) DeleteFileW(toWide(out).c_str());   // never leave a truncated file
    release(key);
    return ok;
}

// ── preview clips ───────────────────────────────────────────────
bool mediaEnsureClip(const std::string& key, const std::string& src) {
    if (mediaHasClip(key)) return true;
    if (!g_hasFfmpeg || !fileExists(src)) return false;
    if (!claim(key)) return false;
    if (mediaHasClip(key)) { release(key); return true; }

    const std::string out = mediaClipFile(key);
    uint64_t dur = mediaDuration(key);
    if (!dur) { dur = probeDuration(src); if (dur) writeDuration(key, dur); }

    double at = dur > 20 ? dur * 0.25 : 0.0;
    char ss[32]; sprintf_s(ss, "%.2f", at);

    // Small, muted, no audio track, faststart so it plays before it finishes
    // downloading. 3 s is enough to tell what a file is.
    std::wstring cmd = L"ffmpeg -y -v error -ss " + toWide(ss) + L" -t 3 -i " + q(src) +
                       L" -an -vf \"scale=320:-2\" -c:v libx264 -preset veryfast -crf 30 "
                       L"-pix_fmt yuv420p -movflags +faststart " + q(out);
    int rc = runHidden(cmd, 90000);

    bool ok = (rc == 0 && fileExists(out));
    if (!ok) DeleteFileW(toWide(out).c_str());
    release(key);
    return ok;
}

// ── background warmer ───────────────────────────────────────────
// Fills the cache gradually so the first visit is not a stampede of ffmpeg
// processes. One file at a time, lowest priority, and it broadcasts once a
// batch lands so open pages pick the thumbnails up.
void mediaWarmerThread() {
    if (!g_hasFfmpeg) return;
    Sleep(2500);                       // let the server finish starting

    while (g_running) {
        std::vector<DbEntry> snapshot;
        { std::lock_guard<std::mutex> lk(g_dbMtx); snapshot = g_database; }

        int made = 0;
        for (auto& e : snapshot) {
            if (!g_running) return;

            std::string name = e.name;
            size_t dot = name.rfind('.');
            std::string ext = (dot == std::string::npos) ? "" : name.substr(dot + 1);
            for (auto& c : ext) c = (char)tolower((unsigned char)c);

            const bool isVid = (ext=="mp4"||ext=="mkv"||ext=="mov"||ext=="avi"||ext=="webm"||
                                ext=="m4v"||ext=="wmv"||ext=="flv"||ext=="mpeg"||ext=="mpg"||
                                ext=="3gp"||ext=="ts"||ext=="m2ts"||ext=="mts");
            const bool isImg = (ext=="jpg"||ext=="jpeg"||ext=="png"||ext=="gif"||ext=="webp"||
                                ext=="bmp"||ext=="tif"||ext=="tiff"||ext=="heic"||ext=="heif"||
                                ext=="avif"||ext=="ico");
            if (!isVid && !isImg) continue;
            if (mediaHasThumb(e.id)) {
                // Durations for videos may still be missing from an older cache.
                if (isVid && !mediaDuration(e.id)) {
                    uint64_t d = probeDuration(e.savedPath);
                    if (d) { writeDuration(e.id, d); ++made; }
                }
                continue;
            }

            if (mediaEnsureThumb(e.id, e.savedPath, isVid)) {
                ++made;
                if (isVid) mediaEnsureClip(e.id, e.savedPath);
            }
            Sleep(120);                // stay out of the way of real requests
            if (made >= 8) break;      // publish progress in batches
        }

        if (made) sseBroadcastDb();
        Sleep(made ? 500 : 15000);     // idle slowly once everything is warm
    }
}
