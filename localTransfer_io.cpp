// ================================================================
//  localTransfer.io.exe  –  Local Network File Transfer
//  Single-file C++ | Windows (WinSock2 + Shell32)
//
//  Architected by: Nicanor III W. Cariasa (2026)
//  Built by: Claude Sonnet 4.6
//
//  Compile (MinGW / MSYS2):
//    g++ -std=c++17 -O2 -o "localTransfer.io.exe" localTransfer_io.cpp -lws2_32 -lshell32 -static-libgcc -static-libstdc++ -mconsole
//
//  Compile (MSVC Developer Prompt):
//    cl /EHsc /std:c++17 /O2 localTransfer_io.cpp ws2_32.lib shell32.lib
//
//  CLI Args:
//    --port / -p <port>            Override port
//    --saving_dir / -Sd <path>     Override saving directory
//    --help / -h                   Show help and exit
//
//  Commands while running:  (use / or \ prefix interchangeably)
//    /help                               Show command list
//    /verbose [0|1]                      Toggle verbose logging
//    /status                             Server status and stats
//    /files                              List all received files
//    /port                               Show active port
//    /ips                                Show all network addresses
//    /saving_dir_change <path>           Change the saving directory
//    /sdc                                Alias for /saving_dir_change
//    /disk_space                         Show disk / storage info
//    /storage_limit <size|off>           Set app storage cap (e.g. 100gb, off)
//    /sl                                 Alias for /storage_limit
//    /database --list/-l                 List database entries
//    /database --reload/-r               Reload + prune stale entries
//    /database --open/-o                 Open database.json in editor
//    /database --clear/-c                Clear all entries (keep files)
//    /database --delete-files/-df        Clear + delete files on disk
//    /database --push <path>             Push a host file into the database
//    /db                                 Alias for /database
//    /forwarding_folder --new/-n <path>  Add a forwarding folder (host only)
//    /forwarding_folder --remove/-rm <id> Remove forwarding folder by id
//    /forwarding_folder --list/-l        List forwarding folders
//    /ff                                 Alias for /forwarding_folder
//    /pastebin --clear                   Clear pastebin
//    /pastebin --overwrite "txt"         Replace pastebin content
//    /pastebin --append "txt"            Append (no separator)
//    /pastebin --append-nl "txt"         Append after two newlines
//    /pastebin --copy                    Copy pastebin to clipboard
//    /pb                                 Alias for /pastebin
//    /clear                              Clear the terminal
//    /exit                               Stop server and quit
// ================================================================

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cctype>
#include <map>
#include <random>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

// ────────────────────────────────────────────────────────────────
//  GLOBALS
// ────────────────────────────────────────────────────────────────
static HANDLE g_hCon = nullptr;
std::atomic<bool>     g_verbose{false};
std::atomic<bool>     g_running{true};
std::atomic<int>      g_activeClients{0};
std::atomic<int>      g_port{0};
std::atomic<uint64_t> g_totalBytes{0};
std::atomic<uint64_t> g_fileCount{0};
std::mutex            g_logMtx;
std::mutex            g_filesMtx;

// Saving directory (default = Desktop, can be changed)
std::string           g_savingDir;
std::mutex            g_savingDirMtx;

// Input-redraw state (g_inputBuf protected by g_logMtx to avoid deadlock)
std::atomic<bool>     g_inputActive{false};
std::string           g_inputBuf; // always accessed under g_logMtx

struct FileRecord { std::string name, savedPath, timestamp, from; uint64_t size = 0; };
std::vector<FileRecord> g_files;

// ── Database entry ──
struct DbEntry {
    std::string id;
    std::string name;
    std::string savedPath;
    std::string timestamp;
    std::string from;
    uint64_t    size{0};
};
std::vector<DbEntry> g_database;
std::mutex           g_dbMtx;

// ── SSE (Server-Sent Events) ──
std::vector<SOCKET>  g_sseClients;
std::mutex           g_sseMtx;

// ── Pastebin (shared across all clients, persisted in config) ──
std::string          g_pastebin;
std::mutex           g_pastebinMtx;

// ── Per-IP connection counting (log only on first connect / last disconnect) ──
std::map<std::string, int> g_ipCount;
std::mutex                 g_ipMtx;

// ── Per-level muting bitmask (bit N = mute LogLvl N) ──
// Default: mute L_NET(6) and L_CMD(5) — user can unmute with /verbose
std::atomic<uint32_t> g_mutedLevels{0}; // 0 = nothing muted

// ── Storage / disk space limits ──
// 0 = no app-level cap (use disk free space * percent)
std::atomic<uint64_t> g_storageLimitBytes{0};
std::atomic<uint64_t> g_uploadLimitPct{80};  // % of available space allowed

// ── Forwarding Folders (host-only) ──
struct ForwardingFolder {
    std::string id;
    std::string path;
    std::vector<std::string> contents; // absolute file paths currently in folder
};
std::vector<ForwardingFolder> g_ffFolders;
std::mutex                    g_ffMtx;

// ────────────────────────────────────────────────────────────────
//  LOGGER  (color-coded, thread-safe, verbose-gated, redraws prompt)
// ────────────────────────────────────────────────────────────────
enum LogLvl { L_INFO=0, L_OK, L_WARN, L_ERR, L_VERB, L_CMD, L_NET };

static const WORD LVL_COLORS[] = { 15, 10, 14, 12, 9, 13, 11 };
static const char* LVL_TAGS[]  = {
    "[INFO]  ", "[OK]    ", "[WARN]  ", "[ERR]   ",
    "[VERB]  ", "[CMD]   ", "[NET]   "
};

static std::string nowStr() {
    SYSTEMTIME s; GetLocalTime(&s);
    char b[16];
    sprintf_s(b, "%02d:%02d:%02d", s.wHour, s.wMinute, s.wSecond);
    return b;
}

static void Log(LogLvl lvl, const std::string& msg) {
    if (lvl == L_VERB && !g_verbose) return;
    // Check per-level mute bitmask
    if (g_mutedLevels.load() & (1u << (unsigned)lvl)) return;

    std::lock_guard<std::mutex> lk(g_logMtx);

    bool wasInput = g_inputActive.load();
    if (wasInput) {
        // Erase current line using Windows console API (avoids wrap artifacts)
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(g_hCon, &csbi)) {
            COORD lineStart = {0, csbi.dwCursorPosition.Y};
            DWORD written = 0;
            FillConsoleOutputCharacterA(g_hCon, ' ', csbi.dwSize.X, lineStart, &written);
            SetConsoleCursorPosition(g_hCon, lineStart);
        }
    }

    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "[" << nowStr() << "] ";
    SetConsoleTextAttribute(g_hCon, LVL_COLORS[lvl]);
    std::cout << LVL_TAGS[lvl];
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << msg << "\n";

    if (wasInput) {
        // Reprint prompt + current buffer (g_inputBuf safe: we hold g_logMtx)
        SetConsoleTextAttribute(g_hCon, 11);
        std::cout << "  localTransfer.io> ";
        SetConsoleTextAttribute(g_hCon, 7);
        std::cout << g_inputBuf;
        std::cout.flush();
    } else {
        std::cout.flush();
    }
}

// ────────────────────────────────────────────────────────────────
//  EXE DIRECTORY
// ────────────────────────────────────────────────────────────────
static std::string getExeDir() {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p(path);
    size_t pos = p.rfind('\\');
    return (pos != std::string::npos) ? p.substr(0, pos) : ".";
}

static std::string getDbPath()     { return getExeDir() + "\\database.json"; }
static std::string getConfigPath() { return getExeDir() + "\\.localTransfer.config"; }

// ────────────────────────────────────────────────────────────────
//  WIN32 WIDE-STRING HELPER  (used throughout for UTF-8 → UTF-16)
// ────────────────────────────────────────────────────────────────
static std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], n);
    return w;
}

// ────────────────────────────────────────────────────────────────
//  DISK SPACE UTILITIES
// ────────────────────────────────────────────────────────────────

// Get the root drive of any path (e.g. "C:\foo\bar" -> "C:\")
static std::string getDiskRoot(const std::string& path) {
    if (path.size() >= 3 && path[1] == ':') {
        return path.substr(0, 3); // e.g. "C:\"
    }
    char full[MAX_PATH] = {};
    GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr);
    std::string fp(full);
    if (fp.size() >= 3 && fp[1] == ':') return fp.substr(0, 3);
    return "C:\\";
}

// Free bytes on the disk that hosts a given path
static uint64_t getDiskFreeBytes(const std::string& path) {
    std::string root = getDiskRoot(path);
    ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFree{};
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvail, &totalBytes, &totalFree))
        return (uint64_t)freeBytesAvail.QuadPart;
    return 0;
}

static uint64_t getDiskTotalBytes(const std::string& path) {
    std::string root = getDiskRoot(path);
    ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFree{};
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvail, &totalBytes, &totalFree))
        return (uint64_t)totalBytes.QuadPart;
    return 0;
}

// Returns the effective max upload allowed right now (re-evaluated each call)
static uint64_t getEffectiveUploadLimitBytes() {
    uint64_t cap = g_storageLimitBytes.load();
    uint64_t pct = g_uploadLimitPct.load();
    uint64_t available = 0;
    if (cap > 0) {
        // Use user-set storage cap as the "available" pool
        available = cap;
    } else {
        // Use actual disk free space
        available = getDiskFreeBytes(getDbPath());
    }
    return (uint64_t)(available * pct / 100ULL);
}

// Sum of all file sizes tracked in the database (the app's actual used storage)
static uint64_t computeDbUsedBytes() {
    std::lock_guard<std::mutex> lk(g_dbMtx);
    uint64_t total = 0;
    for (auto& e : g_database) total += e.size;
    return total;
}

// Parse human-readable size string like "100gb", "500mb", "2tb"
static uint64_t parseHumanSize(const std::string& s) {
    if (s.empty()) return 0;
    std::string lo = s;
    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
    double val = 0;
    try { val = std::stod(lo); } catch (...) { return 0; }
    if      (lo.find("tb") != std::string::npos) return (uint64_t)(val * 1099511627776.0);
    else if (lo.find("gb") != std::string::npos) return (uint64_t)(val * 1073741824.0);
    else if (lo.find("mb") != std::string::npos) return (uint64_t)(val * 1048576.0);
    else if (lo.find("kb") != std::string::npos) return (uint64_t)(val * 1024.0);
    return (uint64_t)val;
}

// ────────────────────────────────────────────────────────────────
//  CONFIG FILE  (.localTransfer.config)
// ────────────────────────────────────────────────────────────────
static std::string loadConfigSavingDir() {
    std::ifstream f(getConfigPath());
    if (!f.is_open()) return "";
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("saving_dir=", 0) == 0)
            return line.substr(11);
    }
    return "";
}

static uint64_t loadConfigStorageLimit() {
    std::ifstream f(getConfigPath());
    if (!f.is_open()) return 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("storage_limit=", 0) == 0) {
            try { return std::stoull(line.substr(14)); } catch (...) {}
        }
    }
    return 0;
}

static void saveConfigSavingDir(const std::string& dir) {
    // Read all existing config
    std::vector<std::string> lines;
    {
        std::ifstream f(getConfigPath());
        std::string l;
        while (std::getline(f, l)) {
            if (l.rfind("saving_dir=", 0) != 0)
                lines.push_back(l);
        }
    }
    std::ofstream f(getConfigPath(), std::ios::trunc);
    f << "saving_dir=" << dir << "\n";
    for (auto& l : lines) f << l << "\n";
}

static void saveConfigStorageLimit(uint64_t bytes) {
    std::vector<std::string> lines;
    {
        std::ifstream f(getConfigPath());
        std::string l;
        while (std::getline(f, l)) {
            if (l.rfind("storage_limit=", 0) != 0)
                lines.push_back(l);
        }
    }
    std::ofstream f(getConfigPath(), std::ios::trunc);
    if (bytes > 0) f << "storage_limit=" << bytes << "\n";
    for (auto& l : lines) f << l << "\n";
}

// Pastebin content stored as a single config key with \n encoded as \\n
static std::string loadConfigPastebin() {
    std::ifstream f(getConfigPath());
    if (!f.is_open()) return "";
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("pastebin=", 0) == 0) {
            std::string enc = line.substr(9);
            // decode \\n -> \n
            std::string out;
            for (size_t i = 0; i < enc.size(); ++i) {
                if (enc[i] == '\\' && i+1 < enc.size() && enc[i+1] == 'n') {
                    out += '\n'; ++i;
                } else out += enc[i];
            }
            return out;
        }
    }
    return "";
}

static void saveConfigPastebin(const std::string& content) {
    std::vector<std::string> lines;
    {
        std::ifstream f(getConfigPath());
        std::string l;
        while (std::getline(f, l)) {
            if (l.rfind("pastebin=", 0) != 0 && l.rfind("saving_dir=", 0) != 0)
                lines.push_back(l);
        }
    }
    // encode \n -> \\n for single-line storage
    std::string enc;
    for (char c : content) {
        if (c == '\n') enc += "\\n";
        else if (c == '\r') {} // skip CR
        else enc += c;
    }
    std::string sd;
    { std::lock_guard<std::mutex> lk(g_savingDirMtx); sd = g_savingDir; }
    std::ofstream f(getConfigPath(), std::ios::trunc);
    f << "saving_dir=" << sd << "\n";
    f << "pastebin=" << enc << "\n";
    for (auto& l : lines) f << l << "\n";
}

// ────────────────────────────────────────────────────────────────
//  JSON HELPERS  (simple, no external libs)
// ────────────────────────────────────────────────────────────────
static std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else                o += c;
    }
    return o;
}

static std::string jsonUnescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i+1 < s.size()) {
            char n = s[i+1];
            if      (n == '"')  { o += '"';  ++i; }
            else if (n == '\\') { o += '\\'; ++i; }
            else if (n == 'n')  { o += '\n'; ++i; }
            else if (n == 'r')  { o += '\r'; ++i; }
            else                { o += n;    ++i; }
        } else { o += s[i]; }
    }
    return o;
}

// Extract string value from a JSON object snippet for key
static std::string jsGetStr(const std::string& obj, const std::string& key) {
    std::string k = "\"" + key + "\":\"";
    size_t p = obj.find(k);
    if (p == std::string::npos) return "";
    p += k.size();
    std::string raw;
    while (p < obj.size()) {
        if (obj[p] == '\\' && p+1 < obj.size()) { raw += obj[p]; raw += obj[p+1]; p += 2; }
        else if (obj[p] == '"') break;
        else { raw += obj[p++]; }
    }
    return jsonUnescape(raw);
}

static uint64_t jsGetU64(const std::string& obj, const std::string& key) {
    std::string k = "\"" + key + "\":";
    size_t p = obj.find(k);
    if (p == std::string::npos) return 0;
    p += k.size();
    while (p < obj.size() && (obj[p]==' '||obj[p]=='\t')) ++p;
    if (p >= obj.size()) return 0;
    try { return (uint64_t)std::stoull(obj.substr(p)); } catch (...) { return 0; }
}

// Split JSON array "[ {...}, {...} ]" into object strings
static std::vector<std::string> jsParseArray(const std::string& json) {
    std::vector<std::string> result;
    size_t pos = json.find('[');
    if (pos == std::string::npos) return result;
    ++pos;
    int depth = 0;
    size_t start = std::string::npos;
    for (; pos < json.size(); ++pos) {
        char c = json[pos];
        if (c == '{') {
            if (depth == 0) start = pos;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                result.push_back(json.substr(start, pos - start + 1));
                start = std::string::npos;
            }
        } else if (c == '"') {
            ++pos;
            while (pos < json.size() && json[pos] != '"') {
                if (json[pos] == '\\') ++pos;
                ++pos;
            }
        }
    }
    return result;
}

// ────────────────────────────────────────────────────────────────
//  DATABASE
// ────────────────────────────────────────────────────────────────
static std::string generateId() {
    SYSTEMTIME s; GetLocalTime(&s);
    char buf[32];
    sprintf_s(buf, "%04d%02d%02d%02d%02d%02d",
        s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond);
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1000, 9999);
    return std::string(buf) + "_" + std::to_string(dist(rng));
}

static void dbSave() {
    std::lock_guard<std::mutex> lkDb(g_dbMtx);
    std::lock_guard<std::mutex> lkFf(g_ffMtx);
    std::ofstream f(getDbPath(), std::ios::trunc);
    if (!f.is_open()) { Log(L_WARN, "Cannot write database.json"); return; }
    // Write main files array
    f << "{\n\"files\":[\n";
    for (size_t i = 0; i < g_database.size(); ++i) {
        auto& e = g_database[i];
        f << "  {";
        f << "\"id\":\"" << jsonEscape(e.id) << "\",";
        f << "\"name\":\"" << jsonEscape(e.name) << "\",";
        f << "\"savedPath\":\"" << jsonEscape(e.savedPath) << "\",";
        f << "\"size\":" << e.size << ",";
        f << "\"timestamp\":\"" << jsonEscape(e.timestamp) << "\",";
        f << "\"from\":\"" << jsonEscape(e.from) << "\"";
        f << "}";
        if (i + 1 < g_database.size()) f << ",";
        f << "\n";
    }
    // Write forwarding_folders array
    f << "],\n\"forwarding_folders\":[\n";
    for (size_t i = 0; i < g_ffFolders.size(); ++i) {
        auto& ff = g_ffFolders[i];
        f << "  {\"id\":\"" << jsonEscape(ff.id) << "\",";
        f << "\"path\":\"" << jsonEscape(ff.path) << "\",";
        f << "\"contents\":[";
        for (size_t j = 0; j < ff.contents.size(); ++j) {
            f << "\"" << jsonEscape(ff.contents[j]) << "\"";
            if (j + 1 < ff.contents.size()) f << ",";
        }
        f << "]}";
        if (i + 1 < g_ffFolders.size()) f << ",";
        f << "\n";
    }
    f << "]\n}\n";
}

// Helper: extract a named JSON array string from an object
static std::string jsGetArrayStr(const std::string& json, const std::string& key) {
    std::string k = "\"" + key + "\":";
    size_t pos = json.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    while (pos < json.size() && (json[pos]==' '||json[pos]=='\t'||json[pos]=='\n'||json[pos]=='\r')) ++pos;
    if (pos >= json.size() || json[pos] != '[') return "";
    int depth = 0;
    size_t start = pos;
    bool inStr = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (inStr) {
            if (c == '\\') ++i;
            else if (c == '"') inStr = false;
        } else {
            if (c == '"') inStr = true;
            else if (c == '[') ++depth;
            else if (c == ']') { --depth; if (depth == 0) return json.substr(start, i - start + 1); }
        }
    }
    return "";
}

static void dbLoad() {
    std::ifstream f(getDbPath());
    if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    std::string filesArr, ffArr;
    // Detect format: new = { "files": [...], "forwarding_folders": [...] }
    //                old = [ {...}, ... ]
    std::string trimmed = content;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && trimmed[first] == '{') {
        // New format
        filesArr = jsGetArrayStr(content, "files");
        ffArr    = jsGetArrayStr(content, "forwarding_folders");
    } else {
        // Old format — plain array of file entries
        filesArr = content;
    }

    // Parse files
    {
        std::lock_guard<std::mutex> lk(g_dbMtx);
        g_database.clear();
        auto objects = jsParseArray(filesArr);
        for (auto& obj : objects) {
            DbEntry e;
            e.id        = jsGetStr(obj, "id");
            e.name      = jsGetStr(obj, "name");
            e.savedPath = jsGetStr(obj, "savedPath");
            e.size      = jsGetU64(obj, "size");
            e.timestamp = jsGetStr(obj, "timestamp");
            e.from      = jsGetStr(obj, "from");
            if (!e.id.empty()) g_database.push_back(e);
        }
    }

    // Parse forwarding folders
    if (!ffArr.empty()) {
        std::lock_guard<std::mutex> lk(g_ffMtx);
        g_ffFolders.clear();
        auto ffObjects = jsParseArray(ffArr);
        for (auto& obj : ffObjects) {
            ForwardingFolder ff;
            ff.id   = jsGetStr(obj, "id");
            ff.path = jsGetStr(obj, "path");
            // parse contents array
            std::string contArr = jsGetArrayStr(obj, "contents");
            if (!contArr.empty()) {
                // Each element is a quoted string "path"
                size_t p = 1; // skip '['
                while (p < contArr.size()) {
                    while (p < contArr.size() && contArr[p] != '"' && contArr[p] != ']') ++p;
                    if (p >= contArr.size() || contArr[p] == ']') break;
                    ++p; // skip opening quote
                    std::string raw;
                    while (p < contArr.size()) {
                        if (contArr[p] == '\\' && p+1 < contArr.size()) { raw += contArr[p]; raw += contArr[p+1]; p += 2; }
                        else if (contArr[p] == '"') { ++p; break; }
                        else { raw += contArr[p++]; }
                    }
                    std::string decoded = jsonUnescape(raw);
                    if (!decoded.empty()) ff.contents.push_back(decoded);
                }
            }
            if (!ff.id.empty()) g_ffFolders.push_back(ff);
        }
    }
}

// Check each path; remove entries whose files no longer exist
static void dbUpdateCheck() {
    std::lock_guard<std::mutex> lk(g_dbMtx);
    size_t before = g_database.size();
    g_database.erase(std::remove_if(g_database.begin(), g_database.end(),
        [](const DbEntry& e) {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, e.savedPath.c_str(), -1, nullptr, 0);
            std::wstring wp(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, e.savedPath.c_str(), -1, &wp[0], wlen);
            return GetFileAttributesW(wp.c_str()) == INVALID_FILE_ATTRIBUTES;
        }), g_database.end());
    size_t removed = before - g_database.size();
    if (removed > 0)
        Log(L_INFO, "Database: removed " + std::to_string(removed) + " stale entries.");
}

static void dbAddEntry(const DbEntry& e) {
    { std::lock_guard<std::mutex> lk(g_dbMtx); g_database.push_back(e); }
    dbSave();
}

// ────────────────────────────────────────────────────────────────
//  SSE BROADCAST
// ────────────────────────────────────────────────────────────────

// Build the full database JSON string (call with g_dbMtx held or copy before calling)
static std::string buildDbJson() {
    std::lock_guard<std::mutex> lk(g_dbMtx);
    std::lock_guard<std::mutex> lkFf(g_ffMtx);
    std::string json = "{\"files\":[";
    for (size_t i = 0; i < g_database.size(); ++i) {
        auto& e = g_database[i];
        json += "{\"id\":\"" + jsonEscape(e.id) + "\",";
        json += "\"name\":\"" + jsonEscape(e.name) + "\",";
        json += "\"size\":" + std::to_string(e.size) + ",";
        json += "\"timestamp\":\"" + jsonEscape(e.timestamp) + "\",";
        json += "\"from\":\"" + jsonEscape(e.from) + "\"}";
        if (i + 1 < g_database.size()) json += ",";
    }
    json += "],\"forwarding_folders\":[";
    for (size_t i = 0; i < g_ffFolders.size(); ++i) {
        auto& ff = g_ffFolders[i];
        json += "{\"id\":\"" + jsonEscape(ff.id) + "\",";
        json += "\"path\":\"" + jsonEscape(ff.path) + "\",";
        json += "\"contents\":[";
        for (size_t j = 0; j < ff.contents.size(); ++j) {
            // Expose basename + full path for download
            std::string fp = ff.contents[j];
            size_t sl = fp.rfind('\\');
            if (sl == std::string::npos) sl = fp.rfind('/');
            std::string fname = (sl != std::string::npos) ? fp.substr(sl+1) : fp;
            json += "{\"name\":\"" + jsonEscape(fname) + "\",";
            json += "\"path\":\"" + jsonEscape(fp) + "\"}";
            if (j + 1 < ff.contents.size()) json += ",";
        }
        json += "]}";
        if (i + 1 < g_ffFolders.size()) json += ",";
    }
    json += "]}";
    return json;
}

static void sseBroadcast(const std::string& event, const std::string& data) {
    // SSE message: event:\ndata:\n\n
    std::string msg = "event: " + event + "\ndata: " + data + "\n\n";
    std::lock_guard<std::mutex> lk(g_sseMtx);
    std::vector<SOCKET> dead;
    for (SOCKET s : g_sseClients) {
        int sent = send(s, msg.data(), (int)msg.size(), 0);
        if (sent <= 0) dead.push_back(s);
    }
    for (SOCKET s : dead) {
        closesocket(s);
        g_sseClients.erase(std::remove(g_sseClients.begin(), g_sseClients.end(), s), g_sseClients.end());
    }
}

static void sseBroadcastDb() {
    sseBroadcast("db_update", buildDbJson());
}

// Send a bare SSE comment line ":ping\n\n" to all clients.
// SSE comment lines (starting with ':') are ignored by EventSource but keep
// the TCP connection alive through mobile radios and proxy idle timeouts.
static void ssePing() {
    const std::string msg = ":ping\n\n";
    std::lock_guard<std::mutex> lk(g_sseMtx);
    std::vector<SOCKET> dead;
    for (SOCKET s : g_sseClients) {
        int sent = send(s, msg.data(), (int)msg.size(), 0);
        if (sent <= 0) dead.push_back(s);
    }
    for (SOCKET s : dead) {
        closesocket(s);
        g_sseClients.erase(std::remove(g_sseClients.begin(), g_sseClients.end(), s), g_sseClients.end());
    }
}

static void sseBroadcastPastebin() {
    std::string content;
    { std::lock_guard<std::mutex> lk(g_pastebinMtx); content = g_pastebin; }
    sseBroadcast("pastebin_update", "{\"content\":\"" + jsonEscape(content) + "\"}");
}

// ────────────────────────────────────────────────────────────────
//  HEARTBEAT THREAD  (500ms: prune stale db paths, broadcast if changed)
// ────────────────────────────────────────────────────────────────
static void heartbeatThread() {
    int pingTick = 0;
    while (g_running) {
        Sleep(500);
        ++pingTick;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lk(g_dbMtx);
            size_t before = g_database.size();
            g_database.erase(std::remove_if(g_database.begin(), g_database.end(),
                [](const DbEntry& e) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, e.savedPath.c_str(), -1, nullptr, 0);
                    std::wstring wp(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, e.savedPath.c_str(), -1, &wp[0], wlen);
                    return GetFileAttributesW(wp.c_str()) == INVALID_FILE_ATTRIBUTES;
                }), g_database.end());
            if (g_database.size() != before) changed = true;
        }

        // ── Scan forwarding folders ──
        {
            std::lock_guard<std::mutex> lk(g_ffMtx);
            for (auto& ff : g_ffFolders) {
                std::vector<std::string> found;
                // Enumerate all files (non-recursive) using wide API for correct UTF-8 handling
                std::wstring wpattern = toWide(ff.path + "\\*");
                WIN32_FIND_DATAW fdW;
                HANDLE h = FindFirstFileW(wpattern.c_str(), &fdW);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        // Convert wide filename to UTF-8
                        char fnUtf8[MAX_PATH * 4] = {};
                        WideCharToMultiByte(CP_UTF8, 0, fdW.cFileName, -1, fnUtf8, sizeof(fnUtf8), nullptr, nullptr);
                        found.push_back(ff.path + "\\" + fnUtf8);
                    } while (FindNextFileW(h, &fdW));
                    FindClose(h);
                }
                if (found != ff.contents) {
                    ff.contents = found;
                    changed = true;
                }
            }
        }

        if (changed) {
            dbSave();
            sseBroadcastDb();
        }

        // Send SSE keepalive ping every 20 seconds
        if (pingTick % 40 == 0) {
            ssePing();
        }
    }
}

// ────────────────────────────────────────────────────────────────
//  EMBEDDED HTML
// ────────────────────────────────────────────────────────────────
static const std::string HTML_PAGE = R"html(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>localTransfer.io</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
:root{
  --bg:#0d1117;
  --surface:#161b22;
  --surface2:#1c2128;
  --border:#30363d;
  --border2:#3d444d;
  --green:#3fb950;
  --green2:#2ea043;
  --green-dim:#1a4a28;
  --blue:#58a6ff;
  --blue-dim:#1f3a5c;
  --ok:#3fb950;
  --warn:#d29922;
  --err:#f85149;
  --txt:#c9d1d9;
  --txt2:#8b949e;
  --dim:#484f58;
  --dim2:#21262d;
}
*{margin:0;padding:0;box-sizing:border-box;}
html{scroll-behavior:smooth;}
body{background:var(--bg);color:var(--txt);font-family:'Inter',sans-serif;min-height:100vh;overflow-x:hidden;}

/* Subtle grain overlay */
body::before{content:'';position:fixed;inset:0;background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='4' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='0.03'/%3E%3C/svg%3E");pointer-events:none;z-index:9999;opacity:.4;}

/* HEADER */
header{display:flex;align-items:center;justify-content:space-between;padding:14px 20px;border-bottom:1px solid var(--border);background:rgba(12,12,14,.97);backdrop-filter:blur(12px);position:sticky;top:0;z-index:100;}
.logo{font-family:'JetBrains Mono',monospace;font-size:1rem;letter-spacing:.04em;color:var(--txt);}
.logo span{color:var(--green);}
.logo em{color:var(--txt2);font-style:normal;}
.header-right{display:flex;align-items:center;gap:8px;}

/* STATUS PILL */
.pill{display:inline-flex;align-items:center;gap:5px;font-size:.65rem;font-family:'JetBrains Mono',monospace;padding:4px 10px;border-radius:20px;border:1px solid;letter-spacing:.05em;}
.pill.online{border-color:var(--ok);color:var(--ok);background:rgba(76,175,125,.07);}
@keyframes blink{0%,100%{opacity:1;}50%{opacity:.25;}}
.pill.online::before{content:'';width:5px;height:5px;border-radius:50%;background:var(--ok);animation:blink 2s infinite;}

/* HAMBURGER MENU */
.menu-wrap{position:relative;}
.menu-btn{font-family:'JetBrains Mono',monospace;font-size:.75rem;padding:6px 10px;border-radius:6px;border:1px solid var(--border2);color:var(--txt2);background:var(--surface2);cursor:pointer;transition:all .2s;display:flex;align-items:center;gap:6px;}
.menu-btn:hover,.menu-btn.open{border-color:var(--green-dim);color:var(--green);background:rgba(63,185,80,.07);}
.menu-dropdown{position:absolute;top:calc(100% + 6px);right:0;background:var(--surface);border:1px solid var(--border2);border-radius:8px;padding:6px;min-width:160px;z-index:200;display:none;box-shadow:0 8px 32px rgba(0,0,0,.5);}
.menu-dropdown.open{display:block;}
.menu-item{display:flex;align-items:center;gap:8px;padding:8px 10px;border-radius:5px;font-size:.78rem;font-family:'JetBrains Mono',monospace;color:var(--txt2);cursor:pointer;transition:all .15s;border:none;background:none;width:100%;text-align:left;}
.menu-item:hover,.menu-item.active{background:rgba(63,185,80,.1);color:var(--green);}
.menu-item-icon{font-size:.9rem;opacity:.7;}
.menu-sep{height:1px;background:var(--border);margin:4px 0;}

/* INFO BUTTON */
.info-btn{font-family:'JetBrains Mono',monospace;font-size:.7rem;padding:6px 10px;border-radius:6px;border:1px solid var(--border2);color:var(--txt2);background:var(--surface2);cursor:pointer;transition:all .2s;}
.info-btn:hover,.info-btn.active{border-color:var(--blue);color:var(--blue);background:rgba(88,166,255,.07);}

/* INFO PANEL (slides in from right) */
.info-panel{position:fixed;top:0;right:-340px;width:320px;height:100vh;background:var(--surface);border-left:1px solid var(--border2);z-index:300;transition:right .3s ease;display:flex;flex-direction:column;overflow:hidden;}
.info-panel.open{right:0;}
.info-panel-header{padding:16px 18px;border-bottom:1px solid var(--border);display:flex;align-items:center;justify-content:space-between;}
.info-panel-header h3{font-size:.7rem;letter-spacing:.15em;text-transform:uppercase;color:var(--dim);font-family:'JetBrains Mono',monospace;}
.info-close{background:none;border:none;color:var(--dim);font-size:1rem;cursor:pointer;padding:2px 6px;border-radius:4px;}
.info-close:hover{color:var(--txt);}
.info-body{flex:1;overflow-y:auto;padding:16px;}
.info-section{margin-bottom:20px;}
.info-section-title{font-size:.6rem;letter-spacing:.15em;text-transform:uppercase;color:var(--dim);font-family:'JetBrains Mono',monospace;margin-bottom:10px;}
.info-row{display:flex;justify-content:space-between;align-items:center;padding:7px 0;border-bottom:1px solid var(--dim2);}
.info-row:last-child{border-bottom:none;}
.info-label{font-size:.72rem;color:var(--txt2);}
.info-value{font-size:.72rem;font-family:'JetBrains Mono',monospace;color:var(--green);}
.info-value.ok{color:var(--ok);}
.info-value.warn{color:var(--warn);}

/* STORAGE BAR */
.storage-bar-wrap{margin-top:10px;}
.storage-bar-labels{display:flex;justify-content:space-between;font-size:.62rem;font-family:'JetBrains Mono',monospace;color:var(--dim);margin-bottom:6px;}
.storage-bar-track{height:5px;background:var(--dim2);border-radius:3px;overflow:hidden;}
.storage-bar-fill{height:100%;background:linear-gradient(90deg,var(--green2),var(--green));border-radius:3px;transition:width .4s;}
.storage-bar-fill.warn{background:linear-gradient(90deg,var(--warn),#e8a060);}
.storage-bar-fill.full{background:linear-gradient(90deg,var(--err),#d46070);}

/* SPEED METERS */
.speed-row{display:flex;gap:10px;margin-top:8px;}
.speed-card{flex:1;background:var(--dim2);border-radius:6px;padding:10px 12px;}
.speed-card-label{font-size:.58rem;letter-spacing:.1em;text-transform:uppercase;color:var(--dim);font-family:'JetBrains Mono',monospace;margin-bottom:4px;}
.speed-card-val{font-size:.85rem;font-family:'JetBrains Mono',monospace;color:var(--green);}

/* STATS BAR */
.statsbar{display:flex;border-bottom:1px solid var(--border);background:var(--surface);}
.stat{flex:1;padding:10px 16px;border-right:1px solid var(--border);}
.stat:last-child{border-right:none;}
.stat label{color:var(--dim);display:block;font-size:.55rem;letter-spacing:.12em;text-transform:uppercase;font-family:'JetBrains Mono',monospace;margin-bottom:3px;}
.stat value{color:var(--green);font-size:.82rem;font-family:'JetBrains Mono',monospace;}

/* LAYOUT */
.layout{display:flex;min-height:calc(100vh - 108px);}
main{flex:1;min-width:0;padding:28px 24px 80px;}

/* SIDEBAR */
.sidebar{width:0;overflow:hidden;border-left:0px solid var(--border);transition:width .3s ease,border-width .3s;background:var(--surface);display:flex;flex-direction:column;}
.sidebar.open{width:360px;border-left:1px solid var(--border);}
.sb-inner{width:360px;padding:16px 14px;flex:1;overflow-y:auto;display:flex;flex-direction:column;}
.sb-panel-header{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px;}
.sb-panel-header h3{margin-bottom:0;}
.sb-close-btn{font-family:'JetBrains Mono',monospace;font-size:.6rem;padding:4px 10px;border-radius:5px;border:1px solid var(--border2);color:var(--txt2);background:transparent;cursor:pointer;transition:all .15s;white-space:nowrap;}
.sb-close-btn:hover{border-color:var(--err);color:var(--err);}
.sb-inner h3{font-family:'JetBrains Mono',monospace;font-size:.6rem;letter-spacing:.18em;text-transform:uppercase;color:var(--dim);}

/* DB ITEMS */
.db-item{padding:11px 13px;border:1px solid var(--border);border-radius:6px;margin-bottom:7px;background:var(--bg);position:relative;overflow:hidden;animation:slidein .2s ease both;transition:border-color .2s;}
.db-item:hover{border-color:var(--green-dim);}
.db-item::before{content:'';position:absolute;left:0;top:0;bottom:0;width:2px;background:linear-gradient(to bottom,var(--green2),var(--green));}
.db-item-name{font-size:.82rem;font-weight:600;color:var(--txt);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-bottom:3px;}
.db-item-meta{font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);margin-bottom:8px;}
.db-item-actions{display:flex;gap:5px;}
.db-btn{font-family:'JetBrains Mono',monospace;font-size:.58rem;padding:3px 9px;border-radius:4px;cursor:pointer;border:1px solid;transition:all .15s;text-decoration:none;display:inline-block;}
.db-btn-dl{border-color:var(--green-dim);color:var(--green2);}
.db-btn-dl:hover{border-color:var(--green);color:var(--green);background:rgba(63,185,80,.08);}
.db-btn-del{border-color:#3a2030;color:var(--err);}
.db-btn-del:hover{background:rgba(201,76,92,.08);border-color:var(--err);}
.sb-empty{font-family:'JetBrains Mono',monospace;font-size:.7rem;color:var(--dim);text-align:center;padding:40px 0;}

/* FF SECTION */
.ff-section{margin-top:16px;}
.ff-section-header{font-family:'JetBrains Mono',monospace;font-size:.58rem;letter-spacing:.15em;text-transform:uppercase;color:var(--blue);margin-bottom:10px;padding-bottom:6px;border-bottom:1px solid var(--border);}
.ff-folder{margin-bottom:10px;}
.ff-folder-path{font-size:.7rem;color:var(--txt2);font-family:'JetBrains Mono',monospace;margin-bottom:6px;padding:5px 8px;background:var(--surface2);border-radius:4px;border-left:2px solid var(--blue);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.ff-file{display:flex;align-items:center;justify-content:space-between;padding:5px 8px;border-bottom:1px solid var(--dim2);font-size:.72rem;}
.ff-file:last-child{border-bottom:none;}
.ff-file-name{color:var(--txt2);}
.ff-dl-btn{font-family:'JetBrains Mono',monospace;font-size:.55rem;padding:2px 7px;border-radius:3px;border:1px solid var(--blue);color:var(--blue);text-decoration:none;transition:all .15s;}
.ff-dl-btn:hover{background:rgba(88,166,255,.12);}

/* PASTEBIN */
.paste-area{flex:1;display:flex;flex-direction:column;gap:8px;}
.paste-textarea{flex:1;min-height:340px;background:var(--bg);border:1px solid var(--border);border-radius:6px;color:var(--txt);font-family:'JetBrains Mono',monospace;font-size:.78rem;padding:12px;resize:vertical;outline:none;line-height:1.6;transition:border-color .2s;}
.paste-textarea:focus{border-color:var(--green-dim);}
.paste-meta{font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);}
.paste-synced{color:var(--ok);}
.paste-syncing{color:var(--warn);}
.paste-actions{display:flex;gap:6px;flex-wrap:wrap;}
.paste-act-btn{font-family:'JetBrains Mono',monospace;font-size:.6rem;padding:4px 10px;border-radius:4px;cursor:pointer;border:1px solid var(--border2);color:var(--dim);background:transparent;transition:all .15s;}
.paste-act-btn:hover{border-color:var(--green-dim);color:var(--green);}

/* DROP ZONE */
h2{font-size:.6rem;letter-spacing:.18em;text-transform:uppercase;color:var(--dim);margin-bottom:14px;font-family:'JetBrains Mono',monospace;}
.dropzone{border:1px solid var(--border);border-radius:10px;padding:40px 28px;text-align:center;cursor:pointer;transition:all .25s;background:var(--surface);position:relative;overflow:hidden;}
.dropzone::after{content:'';position:absolute;inset:0;background:radial-gradient(ellipse at center,rgba(201,168,76,.04) 0%,transparent 70%);pointer-events:none;}
.dropzone:hover,.dropzone.over{border-color:var(--green-dim);box-shadow:0 0 0 1px rgba(63,185,80,.2),inset 0 0 40px rgba(63,185,80,.03);}
.drop-glyph{font-size:2rem;margin-bottom:12px;filter:drop-shadow(0 0 12px rgba(63,185,80,.35));}
.drop-main{font-size:1.1rem;font-weight:600;color:var(--txt);margin-bottom:6px;}
.drop-sub{font-size:.75rem;color:var(--txt2);font-family:'JetBrains Mono',monospace;}

/* ACTIONS */
.actions{display:flex;gap:9px;margin-top:14px;justify-content:center;}
.btn{padding:9px 24px;border-radius:7px;font-family:'Inter',sans-serif;font-weight:600;font-size:.82rem;letter-spacing:.04em;cursor:pointer;transition:all .2s;border:none;}
.btn-primary{background:var(--green);color:#0d1117;}
.btn-primary:hover{background:var(--green2);box-shadow:0 4px 20px rgba(63,185,80,.25);}
#fileInput{display:none;}

/* PROGRESS */
.prog-wrap{margin-top:16px;display:none;}
.prog-header{display:flex;justify-content:space-between;font-family:'JetBrains Mono',monospace;font-size:.65rem;color:var(--dim);margin-bottom:6px;}
.prog-track{height:3px;background:var(--border);border-radius:2px;overflow:hidden;}
.prog-bar{height:100%;width:0%;background:linear-gradient(90deg,var(--green2),var(--green));transition:width .2s;}
.prog-status{font-family:'JetBrains Mono',monospace;font-size:.65rem;color:var(--green);margin-top:5px;text-align:right;}

/* STORAGE STATUS WIDGET */
.storage-widget{margin-bottom:22px;padding:14px 16px;background:var(--surface);border:1px solid var(--border);border-radius:8px;}
.storage-widget-title{font-family:'JetBrains Mono',monospace;font-size:.58rem;letter-spacing:.15em;text-transform:uppercase;color:var(--dim);margin-bottom:10px;}
.storage-nums{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:6px;}
.storage-free{font-family:'JetBrains Mono',monospace;font-size:.8rem;color:var(--green);}
.storage-total{font-family:'JetBrains Mono',monospace;font-size:.65rem;color:var(--dim);}

/* SENT FILE LIST */
.file-list{margin-top:28px;}
@keyframes slidein{from{opacity:0;transform:translateY(-6px);}to{opacity:1;transform:none;}}
.file-item{display:flex;align-items:center;gap:11px;padding:11px 14px;border:1px solid var(--border);border-radius:7px;margin-bottom:7px;background:var(--surface);animation:slidein .25s ease both;position:relative;overflow:hidden;transition:border-color .2s;}
.file-item:hover{border-color:var(--green-dim);}
.file-item::before{content:'';position:absolute;left:0;top:0;bottom:0;width:2px;background:linear-gradient(to bottom,var(--green2),var(--green));}
.ficon{font-size:1.25rem;}
.fmeta{flex:1;min-width:0;}
.fname{font-size:.85rem;font-weight:600;color:var(--txt);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.fdetail{font-family:'JetBrains Mono',monospace;font-size:.6rem;color:var(--dim);margin-top:2px;}
.fbadge{font-family:'JetBrains Mono',monospace;font-size:.58rem;padding:2px 8px;border:1px solid var(--ok);color:var(--ok);border-radius:10px;flex-shrink:0;background:rgba(76,175,125,.07);}

/* TOAST */
.toast{position:fixed;bottom:24px;right:24px;padding:10px 18px;border-radius:7px;font-family:'JetBrains Mono',monospace;font-size:.72rem;pointer-events:none;opacity:0;transition:opacity .3s;z-index:500;border:1px solid;max-width:320px;}
.toast.ok{background:rgba(12,30,20,.97);border-color:var(--ok);color:var(--ok);}
.toast.err{background:rgba(30,12,16,.97);border-color:var(--err);color:var(--err);}
.toast.show{opacity:1;}

/* URLS */
.url-box{margin-bottom:20px;padding:13px 16px;background:var(--surface);border:1px solid var(--border);border-radius:8px;}
.url-item{font-family:'JetBrains Mono',monospace;font-size:.76rem;color:var(--green);padding:2px 0;}
.url-item::before{content:'› ';color:var(--dim);}

/* MOBILE */
@media(max-width:700px){
  header{padding:10px 14px;}
  .logo{font-size:.85rem;}
  .statsbar{flex-wrap:wrap;}
  .stat{min-width:50%;border-right:none;border-bottom:1px solid var(--border);}
  main{padding:18px 14px 60px;}
  .sidebar.open{width:100%;position:fixed;inset:0;z-index:200;overflow-y:auto;}
  .sb-inner{width:100%;}
  .dropzone{padding:28px 16px;}
  .info-panel{width:100%;right:-100%;}
  .info-panel.open{right:0;}
  .storage-widget .storage-nums{flex-direction:column;gap:2px;}
}
@media(max-width:420px){
  .stat{min-width:100%;}
  .actions{flex-direction:column;align-items:stretch;}
  .btn{width:100%;}
}
</style>
</head>
<body>

<header>
  <div class="logo">local<span>Transfer</span><em>.io</em></div>
  <div class="header-right">
    <button class="info-btn" id="infoBtn" onclick="toggleInfo()">⊙ Info</button>
    <div class="menu-wrap">
      <button class="menu-btn" id="menuBtn" onclick="toggleMenu()">☰ <span id="menuLabel">Panels</span></button>
      <div class="menu-dropdown" id="menuDropdown">
        <button class="menu-item" id="menuDb" onclick="openPanel('db')"><span class="menu-item-icon">⬡</span> Database</button>
        <button class="menu-item" id="menuPaste" onclick="openPanel('paste')"><span class="menu-item-icon">⌨</span> Pastebin</button>
      </div>
    </div>
    <div class="pill online">● ONLINE</div>
  </div>
</header>

<div class="statsbar">
  <div class="stat"><label>Active Port</label><value id="sPort">—</value></div>
  <div class="stat"><label>Files Sent</label><value id="sFiles">0</value></div>
  <div class="stat"><label>Data Transferred</label><value id="sBytes">0 B</value></div>
  <div class="stat"><label>Connected</label><value id="sClients">0</value></div>
</div>

<!-- INFO PANEL -->
<div class="info-panel" id="infoPanel">
  <div class="info-panel-header">
    <h3>Connection & Storage</h3>
    <button class="info-close" onclick="toggleInfo()">✕</button>
  </div>
  <div class="info-body">
    <div class="info-section">
      <div class="info-section-title">Storage</div>
      <div class="storage-bar-wrap">
        <div class="storage-bar-labels">
          <span id="iUsed">—</span>
          <span id="iTotal">—</span>
        </div>
        <div class="storage-bar-track"><div class="storage-bar-fill" id="iStorBar" style="width:0%"></div></div>
      </div>
      <div class="info-row" style="margin-top:10px">
        <span class="info-label">Free on disk</span>
        <span class="info-value" id="iFree">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">App storage cap</span>
        <span class="info-value" id="iCap">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">Max upload</span>
        <span class="info-value ok" id="iMaxUp">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">Disk root</span>
        <span class="info-value" id="iDiskRoot">—</span>
      </div>
    </div>
    <div class="info-section">
      <div class="info-section-title">Transfer Speed (live)</div>
      <div class="speed-row">
        <div class="speed-card">
          <div class="speed-card-label">↑ Upload</div>
          <div class="speed-card-val" id="iUpSpeed">—</div>
        </div>
        <div class="speed-card">
          <div class="speed-card-label">↓ Receive</div>
          <div class="speed-card-val" id="iDlSpeed">—</div>
        </div>
      </div>
    </div>
    <div class="info-section">
      <div class="info-section-title">Server</div>
      <div class="info-row">
        <span class="info-label">Port</span>
        <span class="info-value" id="iPort">—</span>
      </div>
      <div class="info-row">
        <span class="info-label">Active clients</span>
        <span class="info-value" id="iClients">—</span>
      </div>
    </div>
  </div>
</div>

<div class="layout">
<main>
  <div class="url-box">
    <h2 style="margin-bottom:8px">Access from any device on this network</h2>
    <div id="urlList">Loading…</div>
  </div>

  <!-- Storage widget -->
  <div class="storage-widget" id="storageWidget">
    <div class="storage-widget-title">Storage</div>
    <div class="storage-nums">
      <span class="storage-free" id="swFree">—</span>
      <span class="storage-total" id="swTotal">—</span>
    </div>
    <div class="storage-bar-track"><div class="storage-bar-fill" id="swBar" style="width:0%"></div></div>
  </div>

  <h2>Upload Files</h2>
  <div class="dropzone" id="dropZone">
    <div class="drop-glyph">⬆</div>
    <div class="drop-main">Drop files here</div>
    <div class="drop-sub" id="dropSub">Any type · Saved to Desktop</div>
  </div>
  <div class="actions">
    <label for="fileInput" class="btn btn-primary">Choose Files</label>
    <input type="file" id="fileInput" multiple>
  </div>
  <div class="prog-wrap" id="progWrap">
    <div class="prog-header"><span id="progLabel">Uploading…</span><span id="progPct">0%</span></div>
    <div class="prog-track"><div class="prog-bar" id="progBar"></div></div>
    <div class="prog-status" id="progStatus">—</div>
  </div>
  <div class="file-list">
    <h2>Sent Files</h2>
    <div id="fileItems"></div>
  </div>
</main>

<aside class="sidebar" id="sidebar">
  <!-- DATABASE PANEL -->
  <div class="sb-inner" id="panelDb" style="display:none">
    <div class="sb-panel-header">
      <h3>Database</h3>
      <button class="sb-close-btn" onclick="closePanel()">✕ Close</button>
    </div>
    <div id="dbItems"><div class="sb-empty">Loading…</div></div>
    <div class="ff-section" id="ffSection" style="display:none">
      <div class="ff-section-header">⟳ Forwarding Folders</div>
      <div id="ffItems"></div>
    </div>
  </div>
  <!-- PASTEBIN PANEL -->
  <div class="sb-inner" id="panelPaste" style="display:none">
    <div class="sb-panel-header">
      <h3>Pastebin <span id="pasteSyncLabel" class="paste-synced" style="margin-left:8px;font-size:.6rem">● synced</span></h3>
      <button class="sb-close-btn" onclick="closePanel()">✕ Close</button>
    </div>
    <div class="paste-area">
      <textarea class="paste-textarea" id="pasteTA" placeholder="Type here — live across all connected devices…" spellcheck="false"></textarea>
      <div class="paste-meta" id="pasteCharCount">0 chars</div>
      <div class="paste-actions">
        <button class="paste-act-btn" onclick="pasteClear()">⌧ clear</button>
        <button class="paste-act-btn" onclick="pasteCopyLocal()">⎘ copy</button>
      </div>
    </div>
  </div>
</aside>
</div>

<div class="toast" id="toast"></div>

<script>
const port = location.port || '80';
document.getElementById('sPort').textContent = port;
document.getElementById('iPort').textContent  = port;

// ── CLOSE MENU ON OUTSIDE CLICK ──
document.addEventListener('click', e => {
  const wrap = document.getElementById('menuBtn').closest('.menu-wrap');
  if (!wrap.contains(e.target)) document.getElementById('menuDropdown').classList.remove('open');
});
function toggleMenu(){
  document.getElementById('menuDropdown').classList.toggle('open');
  document.getElementById('menuBtn').classList.toggle('open');
}

// ── INFO PANEL ──
function toggleInfo(){
  const p = document.getElementById('infoPanel');
  const b = document.getElementById('infoBtn');
  const open = p.classList.toggle('open');
  b.classList.toggle('active', open);
  if (open) loadDiskSpace();
}
let diskPollInterval = null;
function loadDiskSpace(){
  fetch('/api/disk_space').then(r=>r.json()).then(d=>{
    const freeB  = Number(d.disk_free);
    const totB   = Number(d.disk_total);
    const capB   = Number(d.storage_cap);
    const maxUp  = Number(d.max_upload);
    const dbUsed = Number(d.db_used || 0);  // actual sum of database file sizes

    // Pool = cap if set, otherwise total disk
    const pool = capB > 0 ? capB : totB;
    // "used" = db_used (app files only), shown against the pool
    const pct  = pool > 0 ? Math.min(100, dbUsed / pool * 100) : 0;

    document.getElementById('iFree').textContent    = fmtBytes(freeB);
    document.getElementById('iCap').textContent     = capB > 0 ? fmtBytes(capB) : 'None';
    document.getElementById('iMaxUp').textContent   = fmtBytes(maxUp);
    document.getElementById('iDiskRoot').textContent= d.disk_root || '—';
    document.getElementById('iClients').textContent = document.getElementById('sClients').textContent;

    const bar = document.getElementById('iStorBar');
    bar.style.width = pct.toFixed(1) + '%';
    bar.className = 'storage-bar-fill' + (pct>90?' full':pct>70?' warn':'');

    document.getElementById('iUsed').textContent  = fmtBytes(dbUsed) + ' used';
    document.getElementById('iTotal').textContent = fmtBytes(pool);

    // Main storage widget
    document.getElementById('swFree').textContent  = fmtBytes(freeB) + ' free';
    document.getElementById('swTotal').textContent = capB > 0 ? ('Cap: '+fmtBytes(capB)) : fmtBytes(totB);
    const swBar = document.getElementById('swBar');
    swBar.style.width = pct.toFixed(1) + '%';
    swBar.className = 'storage-bar-fill' + (pct>90?' full':pct>70?' warn':'');
  }).catch(()=>{});
}
// Poll disk space every 5s for live update
setInterval(loadDiskSpace, 5000);
loadDiskSpace();

// ── SPEED TRACKING ──
let lastBytes = 0, lastTime = Date.now(), lastDlBytes = 0;
function updateSpeed(){
  const now = Date.now();
  const bytes = Number(document.getElementById('sBytes').dataset.raw || 0);
  const dt = (now - lastTime) / 1000;
  if (dt > 0 && lastBytes > 0) {
    const spd = (bytes - lastBytes) / dt;
    document.getElementById('iUpSpeed').textContent = fmtBytes(spd) + '/s';
  }
  lastBytes = bytes;
  lastTime  = now;
}
setInterval(updateSpeed, 2000);

// ── INFO ──
fetch('/api/info').then(r=>r.json()).then(d=>{
  document.getElementById('urlList').innerHTML = d.ips.map(ip=>`<div class="url-item">http://${ip}:${port}/</div>`).join('');
  if (d.saving_dir) document.getElementById('dropSub').textContent = `Any type · Saved to ${d.saving_dir}`;
}).catch(()=>{ document.getElementById('urlList').innerHTML = `<div class="url-item">http://${location.hostname}:${port}/</div>`; });

// ── STATS POLL ──
function updateStats(){
  fetch('/api/stats').then(r=>r.json()).then(d=>{
    document.getElementById('sFiles').textContent   = d.files;
    const bytesEl = document.getElementById('sBytes');
    bytesEl.textContent = fmtBytes(d.bytes);
    bytesEl.dataset.raw = d.bytes;
    document.getElementById('sClients').textContent = d.clients;
    document.getElementById('iClients').textContent = d.clients;
  }).catch(()=>{});
}
setInterval(updateStats, 4000);
updateStats();

// ── SSE LIVE UPDATES ──
// Robust SSE with:
//   • Proper teardown before every reconnect (no zombie connections)
//   • Watchdog: reconnects if no ping/event received in 35s (server pings every 20s)
//   • visibilitychange: instant reconnect when phone wakes up / tab becomes active
//   • Exponential back-off capped at 15s
let sse = null;
let sseWatchdog = null;
let sseBackoff = 1000; // ms, doubles on each failed attempt up to 15s
const SSE_TIMEOUT = 35000; // ms — must be > server ping interval (20s)

function resetSseWatchdog() {
  clearTimeout(sseWatchdog);
  sseWatchdog = setTimeout(() => {
    // No ping or data arrived in time — assume connection is silently dead
    reconnectSSE();
  }, SSE_TIMEOUT);
}

function teardownSSE() {
  clearTimeout(sseWatchdog);
  if (sse) {
    sse.onerror = null; // prevent onerror from firing during close
    sse.close();
    sse = null;
  }
}

function connectSSE() {
  teardownSSE();
  sse = new EventSource('/events');

  sse.addEventListener('db_update', e => {
    sseBackoff = 1000; // successful data — reset back-off
    resetSseWatchdog();
    try {
      const d = JSON.parse(e.data);
      renderDatabase(d.files||[], d.forwarding_folders||[]);
    } catch(_){}
  });

  sse.addEventListener('pastebin_update', e => {
    sseBackoff = 1000;
    resetSseWatchdog();
    try {
      const d = JSON.parse(e.data);
      const ta = document.getElementById('pasteTA');
      if (!pasteLocalEdit) { ta.value = d.content; pasteUpdateMeta(); }
    } catch(_){}
  });

  // onopen fires when connection is (re-)established
  sse.onopen = () => {
    sseBackoff = 1000;
    resetSseWatchdog();
  };

  sse.onerror = () => {
    teardownSSE();
    setTimeout(connectSSE, Math.min(sseBackoff, 15000));
    sseBackoff = Math.min(sseBackoff * 2, 15000);
  };

  // Server sends ":ping\n\n" SSE comments every 20s.
  // EventSource fires an unnamed "message" event for ":ping" comment lines —
  // we listen to onmessage as a catch-all to reset the watchdog on any traffic.
  // (SSE comment lines don't trigger named event listeners.)
  sse.onmessage = () => {
    sseBackoff = 1000;
    resetSseWatchdog();
  };

  resetSseWatchdog();
}

function reconnectSSE() {
  teardownSSE();
  connectSSE();
}

connectSSE();

// ── RECONNECT ON WAKE-UP ──
// When the phone comes back from sleep or the tab becomes visible again,
// force an immediate SSE reconnect instead of waiting for the watchdog.
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible') {
    reconnectSSE();
    // Also refresh data immediately on wake-up
    loadDiskSpace();
    updateStats();
  }
});

// ── PANEL MANAGEMENT ──
let activePanel = null;
function openPanel(which){
  document.getElementById('menuDropdown').classList.remove('open');
  document.getElementById('menuBtn').classList.remove('open');
  const sidebar = document.getElementById('sidebar');
  const panelDb   = document.getElementById('panelDb');
  const panelPaste= document.getElementById('panelPaste');
  const mDb = document.getElementById('menuDb');
  const mPaste = document.getElementById('menuPaste');

  if (activePanel === which) { closePanel(); return; }
  activePanel = which;
  sidebar.classList.add('open');
  mDb.classList.toggle('active', which==='db');
  mPaste.classList.toggle('active', which==='paste');

  if (which === 'db') {
    panelDb.style.display='flex'; panelDb.style.flexDirection='column';
    panelPaste.style.display='none';
    loadDatabase();
  } else {
    panelPaste.style.display='flex'; panelPaste.style.flexDirection='column';
    panelDb.style.display='none';
    loadPastebin();
  }
}
function closePanel(){
  activePanel = null;
  document.getElementById('sidebar').classList.remove('open');
  document.getElementById('menuDb').classList.remove('active');
  document.getElementById('menuPaste').classList.remove('active');
  document.getElementById('panelDb').style.display='none';
  document.getElementById('panelPaste').style.display='none';
}

// ── FILENAME ENCODING ──
// The server stores Windows-safe encoded filenames on disk and in database.json.
// Illegal chars are substituted with fullwidth Unicode equivalents:
//   | → ｜  * → ＊  ? → ？  " → ＂  < → ＜  > → ＞  : → ：
// decodeDisplayName() reverses this for display and the download= attribute.
function decodeDisplayName(name) {
  return String(name)
    .replace(/｜/g, '|')
    .replace(/＊/g, '*')
    .replace(/？/g, '?')
    .replace(/＂/g, '"')
    .replace(/＜/g, '<')
    .replace(/＞/g, '>')
    .replace(/：/g, ':');
}

// ── DATABASE ──
function loadDatabase(){
  fetch('/api/database').then(r=>r.json()).then(d=>{
    renderDatabase(d.files||[], d.forwarding_folders||[]);
  }).catch(()=>{ document.getElementById('dbItems').innerHTML='<div class="sb-empty">Error loading</div>'; });
}
function renderDatabase(files, ffs){
  const el = document.getElementById('dbItems');
  if(!files.length){ el.innerHTML='<div class="sb-empty">No files in database</div>'; }
  else {
    el.innerHTML = files.map(f=>{
      // name in DB is encoded (matches disk); decode for display and download attr
      const displayName = decodeDisplayName(f.name);
      return `
      <div class="db-item" id="dbItem_${f.id}">
        <div class="db-item-name">${escHtml(displayName)}</div>
        <div class="db-item-meta">${fmtBytes(f.size)} · ${escHtml(f.timestamp)} · from ${escHtml(f.from)}</div>
        <div class="db-item-actions">
          <a class="db-btn db-btn-dl" href="/download?id=${encodeURIComponent(f.id)}" download="${escHtml(displayName)}">↓ Download</a>
          <button class="db-btn db-btn-del" onclick="dbDelete('${f.id}')">✕ Delete</button>
        </div>
      </div>`;
    }).join('');
  }

  // Forwarding folders
  const ffSec = document.getElementById('ffSection');
  const ffEl  = document.getElementById('ffItems');
  if (ffs && ffs.length > 0) {
    ffSec.style.display = 'block';
    ffEl.innerHTML = ffs.map(ff => {
      const ffItems = (ff.contents||[]).length === 0
        ? '<div style="font-size:.65rem;color:var(--dim);padding:4px 8px">Empty folder</div>'
        : (ff.contents||[]).map(c => {
            // c.name is the encoded disk basename; decode for display and download attr
            const displayName = decodeDisplayName(c.name||c);
            return `
          <div class="ff-file">
            <span class="ff-file-name">${escHtml(displayName)}</span>
            <a class="ff-dl-btn" href="/download_ff?path=${encodeURIComponent(c.path||c)}" download="${escHtml(displayName)}">↓</a>
          </div>`;
          }).join('');
      return `
      <div class="ff-folder">
        <div class="ff-folder-path" title="${escHtml(ff.path)}">${escHtml(ff.path)}</div>
        ${ffItems}
      </div>`;
    }).join('');
  } else {
    ffSec.style.display = 'none';
  }
}
function dbDelete(id){
  if(!confirm('Delete this file from the host?')) return;
  fetch('/api/database/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id,delete_file:true})})
  .then(r=>r.json()).then(d=>{
    if(d.success){ const el=document.getElementById('dbItem_'+id); if(el) el.remove(); showToast('✓ File deleted'); }
    else showToast('Delete failed: '+(d.error||'unknown'),true);
  }).catch(()=>showToast('Network error',true));
}

// ── PASTEBIN ──
let pasteLocalEdit = false, pasteEditTimer = null, pasteSyncTimer = null;
function loadPastebin(){
  fetch('/api/pastebin').then(r=>r.json()).then(d=>{
    document.getElementById('pasteTA').value = d.content||'';
    pasteUpdateMeta();
  }).catch(()=>{});
}
function pasteUpdateMeta(){
  document.getElementById('pasteCharCount').textContent = document.getElementById('pasteTA').value.length + ' chars';
}
function pasteSyncNow(){
  const content = document.getElementById('pasteTA').value;
  setSyncLabel(false);
  fetch('/api/pastebin',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({content})})
  .then(()=>{ pasteLocalEdit=false; setSyncLabel(true); }).catch(()=>setSyncLabel(true));
}
function setSyncLabel(synced){
  const el = document.getElementById('pasteSyncLabel');
  el.textContent = synced ? '● synced' : '⟳ syncing…';
  el.className = synced ? 'paste-synced' : 'paste-syncing';
}
document.getElementById('pasteTA').addEventListener('input', ()=>{
  pasteLocalEdit = true; pasteUpdateMeta(); setSyncLabel(false);
  clearTimeout(pasteSyncTimer); pasteSyncTimer = setTimeout(pasteSyncNow, 300);
  clearTimeout(pasteEditTimer); pasteEditTimer = setTimeout(()=>{ pasteLocalEdit=false; }, 600);
});
function pasteClear(){ document.getElementById('pasteTA').value=''; pasteUpdateMeta(); pasteSyncNow(); }
function pasteCopyLocal(){ navigator.clipboard.writeText(document.getElementById('pasteTA').value).then(()=>showToast('✓ Copied')); }

// ── UPLOAD ──
function uploadFiles(files){
  if(!files||!files.length) return;
  const meta={};
  for(const f of files) meta[f.name]={lastModified:f.lastModified,size:f.size};
  const fd=new FormData();
  for(const f of files) fd.append('files',f);
  fd.append('metadata',JSON.stringify(meta));
  const wrap=document.getElementById('progWrap');
  const bar=document.getElementById('progBar');
  const pct=document.getElementById('progPct');
  const lbl=document.getElementById('progLabel');
  const sts=document.getElementById('progStatus');
  wrap.style.display='block'; bar.style.width='0%'; pct.textContent='0%';
  lbl.textContent=`Uploading ${files.length} file${files.length>1?'s':''}…`;
  const xhr=new XMLHttpRequest();
  let startTime = Date.now(), lastLoaded = 0;
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){
      const p=Math.round(e.loaded/e.total*100);
      bar.style.width=p+'%'; pct.textContent=p+'%';
      const dt=(Date.now()-startTime)/1000;
      const spd = dt>0 ? (e.loaded-lastLoaded)/Math.max(0.5,dt) : 0;
      sts.textContent=fmtBytes(e.loaded)+' / '+fmtBytes(e.total)+(spd>0?' · '+fmtBytes(e.loaded/Math.max(0.1,dt))+'/s':'');
      document.getElementById('iUpSpeed').textContent = fmtBytes(e.loaded/Math.max(0.1,dt)) + '/s';
    }
  };
  xhr.onload=()=>{
    wrap.style.display='none';
    if(xhr.status===200){
      try{
        const res=JSON.parse(xhr.responseText);
        for(const f of res.files) addFileCard(f.name,f.size,f.ts,res.saving_dir);
        showToast('✓ '+res.files.length+' file(s) saved');
        updateStats(); loadDiskSpace();
      }catch(e){showToast('Parse error',true);}
    } else showToast('Upload failed: HTTP '+xhr.status,true);
  };
  xhr.onerror=()=>{wrap.style.display='none';showToast('Network error',true);};
  xhr.open('POST','/upload'); xhr.send(fd);
}
function addFileCard(name,size,ts,savingDir){
  const div=document.createElement('div'); div.className='file-item';
  div.innerHTML=`<div class="ficon">${fileIcon(name)}</div>
    <div class="fmeta"><div class="fname">${escHtml(name)}</div>
    <div class="fdetail">${fmtBytes(size)} · ${ts} · ${escHtml(savingDir||'Desktop')}</div></div>
    <div class="fbadge">✓ SAVED</div>`;
  document.getElementById('fileItems').prepend(div);
  const sf = document.getElementById('sFiles');
  sf.textContent = parseInt(sf.textContent||0)+1;
}

// ── DRAG & DROP ──
const dz = document.getElementById('dropZone');
dz.addEventListener('dragover', e=>{ e.preventDefault(); dz.classList.add('over'); });
dz.addEventListener('dragleave', ()=>dz.classList.remove('over'));
dz.addEventListener('drop', e=>{ e.preventDefault(); dz.classList.remove('over'); uploadFiles(e.dataTransfer.files); });
dz.addEventListener('click', ()=>document.getElementById('fileInput').click());
document.getElementById('fileInput').addEventListener('change', e=>uploadFiles(e.target.files));

// ── UTILS ──
function showToast(msg,isErr=false){
  const t=document.getElementById('toast');
  t.textContent=msg; t.className='toast '+(isErr?'err':'ok')+' show';
  setTimeout(()=>t.className='toast',4000);
}
function fmtBytes(b){
  b=Number(b);
  if(b<1024) return b+' B';
  if(b<1048576) return (b/1024).toFixed(1)+' KB';
  if(b<1073741824) return (b/1048576).toFixed(1)+' MB';
  return (b/1073741824).toFixed(2)+' GB';
}
function escHtml(s){ return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'); }
function fileIcon(name){
  const e=(name||'').split('.').pop().toLowerCase();
  const m={png:'🖼',jpg:'🖼',jpeg:'🖼',gif:'🖼',webp:'🖼',mp4:'🎬',mov:'🎬',avi:'🎬',mkv:'🎬',mp3:'🎵',wav:'🎵',flac:'🎵',pdf:'📄',doc:'📝',docx:'📝',txt:'📝',md:'📝',zip:'🗜',rar:'🗜','7z':'🗜',tar:'🗜',xls:'📊',xlsx:'📊',csv:'📊',exe:'⚙',apk:'📱',psd:'🎨',svg:'🎨',js:'💻',ts:'💻',py:'💻',cpp:'💻',rs:'💻',go:'💻'};
  return m[e]||'📁';
}
</script>
</body>
</html>
)html";


// ────────────────────────────────────────────────────────────────
//  UTILITIES
// ────────────────────────────────────────────────────────────────

static std::string getDesktopPath() {
    WCHAR path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path))) {
        char narrow[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
        return std::string(narrow);
    }
    return ".";
}

static std::string getSavingDir() {
    std::lock_guard<std::mutex> lk(g_savingDirMtx);
    return g_savingDir;
}

static void setSavingDir(const std::string& dir) {
    { std::lock_guard<std::mutex> lk(g_savingDirMtx); g_savingDir = dir; }
    saveConfigSavingDir(dir);
}

static std::vector<std::string> getLocalIPs() {
    std::vector<std::string> ips;
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) != 0) return ips;
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) return ips;
    for (auto* p = res; p; p = p->ai_next) {
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &((sockaddr_in*)p->ai_addr)->sin_addr, ip, sizeof(ip));
        std::string s(ip);
        if (s != "127.0.0.1") ips.push_back(s);
    }
    freeaddrinfo(res);
    if (ips.empty()) ips.push_back("127.0.0.1");
    return ips;
}

static std::string formatBytes(uint64_t b) {
    char buf[64];
    if (b < 1024)            sprintf_s(buf, "%llu B",  b);
    else if (b < 1048576)    sprintf_s(buf, "%.1f KB", b/1024.0);
    else if (b < 1073741824) sprintf_s(buf, "%.2f MB", b/1048576.0);
    else                     sprintf_s(buf, "%.2f GB", b/1073741824.0);
    return buf;
}

static std::string nowHuman() {
    SYSTEMTIME s; GetLocalTime(&s);
    char b[32];
    sprintf_s(b, "%04d-%02d-%02d %02d:%02d:%02d",
        s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond);
    return b;
}

static size_t findSeq(const std::vector<uint8_t>& buf, size_t start, const std::string& pat) {
    if (pat.empty() || buf.size() < pat.size()) return std::string::npos;
    for (size_t i = start; i + pat.size() <= buf.size(); ++i) {
        if (memcmp(buf.data() + i, pat.data(), pat.size()) == 0) return i;
    }
    return std::string::npos;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::string getHeaderVal(const std::string& headers, const std::string& name) {
    std::string h = headers, n = name;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    size_t pos = h.find(n);
    if (pos == std::string::npos) return "";
    pos = h.find(':', pos);
    if (pos == std::string::npos) return "";
    size_t eol = h.find('\n', pos);
    std::string val = headers.substr(pos + 1, eol == std::string::npos ? std::string::npos : eol - pos - 1);
    return trim(val);
}

static std::string extractFilename(const std::string& cd) {
    size_t pos = cd.find("filename*=");
    if (pos != std::string::npos) {
        pos = cd.find("''", pos);
        if (pos != std::string::npos) {
            std::string enc = cd.substr(pos + 2);
            std::string dec;
            for (size_t i = 0; i < enc.size(); ++i) {
                if (enc[i] == '%' && i + 2 < enc.size()) {
                    char hex[3] = {enc[i+1], enc[i+2], 0};
                    dec += (char)strtol(hex, nullptr, 16); i += 2;
                } else dec += enc[i];
            }
            return trim(dec);
        }
    }
    pos = cd.find("filename=");
    if (pos == std::string::npos) return "";
    pos += 9;
    if (pos < cd.size() && cd[pos] == '"') {
        size_t end = cd.find('"', pos + 1);
        if (end != std::string::npos) return cd.substr(pos + 1, end - pos - 1);
    }
    size_t end = cd.find_first_of(";\r\n", pos);
    return trim(cd.substr(pos, end == std::string::npos ? std::string::npos : end - pos));
}

static int64_t extractJsonInt64(const std::string& json, const std::string& filename, const std::string& key) {
    std::string fkey = "\"" + filename + "\"";
    size_t fi = json.find(fkey);
    if (fi == std::string::npos) return 0;
    std::string kkey = "\"" + key + "\"";
    size_t ki = json.find(kkey, fi);
    if (ki == std::string::npos) return 0;
    size_t ci = json.find(':', ki + kkey.size());
    if (ci == std::string::npos) return 0;
    ++ci;
    while (ci < json.size() && (json[ci]==' '||json[ci]=='\t')) ++ci;
    if (ci >= json.size()) return 0;
    try { return (int64_t)std::stoll(json.substr(ci)); } catch (...) { return 0; }
}

static void setFileTimes(const std::string& path, int64_t lastModifiedMs) {
    if (lastModifiedMs <= 0) return;
    LONGLONG ll = (LONGLONG)lastModifiedMs * 10000LL + 116444736000000000LL;
    FILETIME ft;
    ft.dwLowDateTime  = (DWORD)(ll & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD)(ll >> 32);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    HANDLE h = CreateFileW(wpath.c_str(), FILE_WRITE_ATTRIBUTES, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { SetFileTime(h, &ft, &ft, &ft); CloseHandle(h); }
}

// ────────────────────────────────────────────────────────────────
//  FILENAME ENCODER / DECODER
//  Maps Windows-illegal chars to visually-similar fullwidth Unicode
//  equivalents that are valid in NTFS.  The mapping is reversible so
//  the original name can be reconstructed client-side (and in the
//  Content-Disposition header) without any lossy substitution.
//
//  Mapping (UTF-8 byte sequences):
//    |  (0x7C)  →  ｜  U+FF5C  EF BD 9C
//    *  (0x2A)  →  ＊  U+FF0A  EF BC 8A
//    ?  (0x3F)  →  ？  U+FF1F  EF BC 9F
//    "  (0x22)  →  ＂  U+FF02  EF BC 82
//    <  (0x3C)  →  ＜  U+FF1C  EF BC 9C
//    >  (0x3E)  →  ＞  U+FF1E  EF BC 9E
//    :  (0x3A)  →  ：  U+FF1A  EF BC 9A
// ────────────────────────────────────────────────────────────────
static std::string encodeFilenameForDisk(const std::string& name) {
    std::string out;
    out.reserve(name.size() * 2);
    for (unsigned char c : name) {
        switch (c) {
            case '|': out += "\xEF\xBD\x9C"; break;  // ｜ U+FF5C
            case '*': out += "\xEF\xBC\x8A"; break;  // ＊ U+FF0A
            case '?': out += "\xEF\xBC\x9F"; break;  // ？ U+FF1F
            case '"': out += "\xEF\xBC\x82"; break;  // ＂ U+FF02
            case '<': out += "\xEF\xBC\x9C"; break;  // ＜ U+FF1C
            case '>': out += "\xEF\xBC\x9E"; break;  // ＞ U+FF1E
            case ':': out += "\xEF\xBC\x9A"; break;  // ： U+FF1A
            default:  out += (char)c;         break;
        }
    }
    return out;
}

static std::string decodeFilenameFromDisk(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ) {
        unsigned char c  = (unsigned char)name[i];
        if (c == 0xEF && i + 2 < name.size()) {
            unsigned char b1 = (unsigned char)name[i+1];
            unsigned char b2 = (unsigned char)name[i+2];
            if      (b1 == 0xBD && b2 == 0x9C) { out += '|'; i += 3; continue; } // ｜
            else if (b1 == 0xBC && b2 == 0x8A) { out += '*'; i += 3; continue; } // ＊
            else if (b1 == 0xBC && b2 == 0x9F) { out += '?'; i += 3; continue; } // ？
            else if (b1 == 0xBC && b2 == 0x82) { out += '"'; i += 3; continue; } // ＂
            else if (b1 == 0xBC && b2 == 0x9C) { out += '<'; i += 3; continue; } // ＜
            else if (b1 == 0xBC && b2 == 0x9E) { out += '>'; i += 3; continue; } // ＞
            else if (b1 == 0xBC && b2 == 0x9A) { out += ':'; i += 3; continue; } // ：
        }
        out += name[i++];
    }
    return out;
}

static std::string sanitizeFilename(const std::string& name) {
    // Encode illegal chars to fullwidth Unicode equivalents
    std::string encoded = encodeFilenameForDisk(name);
    // Strip path separators (/ and \) — they are not encodable as single chars
    std::string out;
    for (char c : encoded) {
        if (c == '/' || c == '\\') out += '_';
        else out += c;
    }
    if (out.empty() || out == "." || out == "..") out = "upload";
    return out;
}

static std::string uniquePath(const std::string& dir, const std::string& name) {
    std::string path = dir + "\\" + name;
    if (GetFileAttributesW(toWide(path).c_str()) == INVALID_FILE_ATTRIBUTES) return path;
    size_t dot = name.rfind('.');
    std::string base = (dot == std::string::npos) ? name : name.substr(0, dot);
    std::string ext  = (dot == std::string::npos) ? ""   : name.substr(dot);
    for (int i = 2; i < 9999; ++i) {
        path = dir + "\\" + base + "(" + std::to_string(i) + ")" + ext;
        if (GetFileAttributesW(toWide(path).c_str()) == INVALID_FILE_ATTRIBUTES) return path;
    }
    return dir + "\\" + name;
}

// URL decode a query string value
static std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') { out += ' '; }
        else if (s[i] == '%' && i+2 < s.size()) {
            char hex[3] = {s[i+1], s[i+2], 0};
            out += (char)strtol(hex, nullptr, 16);
            i += 2;
        } else { out += s[i]; }
    }
    return out;
}

// Parse query string: extract value for key
static std::string queryParam(const std::string& path, const std::string& key) {
    size_t q = path.find('?');
    if (q == std::string::npos) return "";
    std::string qs = path.substr(q + 1);
    std::string k = key + "=";
    size_t pos = qs.find(k);
    if (pos == std::string::npos) return "";
    pos += k.size();
    size_t end = qs.find('&', pos);
    std::string val = qs.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    return urlDecode(val);
}

// ────────────────────────────────────────────────────────────────
//  HTTP HELPERS
// ────────────────────────────────────────────────────────────────

static bool recvAll(SOCKET s, std::string& headers, std::vector<uint8_t>& body) {
    std::string raw;
    raw.reserve(8192);
    char buf[65536];
    for (;;) {
        int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        raw.append(buf, n);
        auto pos = raw.find("\r\n\r\n");
        if (pos != std::string::npos) {
            headers = raw.substr(0, pos);
            std::string tail = raw.substr(pos + 4);
            body.assign(tail.begin(), tail.end());
            break;
        }
        if (raw.size() > 128 * 1024) return false; // header too large
    }
    std::string cl = getHeaderVal(headers, "Content-Length");
    if (cl.empty()) return true;
    int64_t need = 0;
    try { need = std::stoll(cl); } catch (...) { return false; }
    Log(L_VERB, "  Content-Length: " + cl + " bytes");

    // Pre-reserve to avoid O(n^2) insertions
    if (need > 0 && need <= (int64_t)4ULL * 1024 * 1024 * 1024) { // cap at 4GB
        try { body.reserve((size_t)need); } catch (...) { return false; }
    }

    // For large uploads, remove the per-recv timeout so slow connections don't get cut off
    if (need > (int64_t)32 * 1024 * 1024) { // > 32 MB
        DWORD noTimeout = 0; // infinite
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&noTimeout, sizeof(noTimeout));
    }

    while ((int64_t)body.size() < need) {
        int64_t rem = need - (int64_t)body.size();
        int chunk = (int)std::min(rem, (int64_t)sizeof(buf));
        int n = recv(s, buf, chunk, 0);
        if (n <= 0) break;
        body.insert(body.end(), buf, buf + n);
    }
    return (int64_t)body.size() >= need;
}

static void sendResp(SOCKET s, int code, const std::string& ctype,
                     const std::string& body, const std::string& extraHdrs = "") {
    const char* status = (code == 200) ? "OK" : (code == 404) ? "Not Found" : "Bad Request";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << status << "\r\n"
        << "Content-Type: "   << ctype  << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << extraHdrs << "\r\n" << body;
    std::string resp = oss.str();
    send(s, resp.data(), (int)resp.size(), 0);
}

// Stream a file download.
// filePath = actual path on disk (UTF-8, may contain fullwidth-encoded chars).
// diskName = the filename as it exists on disk (encoded, e.g. "hello ｜ world.mp4").
//            This is what Content-Disposition sends — the browser's download= attribute
//            on the <a> tag (set client-side to the decoded original) takes precedence
//            for UI-triggered downloads on same-origin requests.
static void sendFileDownload(SOCKET s, const std::string& filePath, const std::string& diskName) {
    HANDLE hf = CreateFileW(toWide(filePath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        sendResp(s, 404, "text/plain", "File not found");
        return;
    }
    LARGE_INTEGER sz; GetFileSizeEx(hf, &sz);

    // Percent-encode the disk name (UTF-8 bytes) for the RFC 5987 filename* parameter.
    // This keeps the encoded fullwidth chars intact in the header as %XX sequences.
    std::string rfc5987;
    for (unsigned char c : diskName) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            rfc5987 += (char)c;
        } else {
            char hex[4];
            sprintf_s(hex, "%%%02X", (unsigned)c);
            rfc5987 += hex;
        }
    }
    // ASCII-safe fallback for the plain filename= parameter
    std::string asciiName;
    for (unsigned char c : diskName) {
        if (c >= 32 && c < 127 && c != '"' && c != '\\') asciiName += (char)c;
        else asciiName += '_';
    }
    if (asciiName.empty()) asciiName = "download";

    std::ostringstream hdr;
    hdr << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/octet-stream\r\n"
        << "Content-Disposition: attachment; filename=\"" << asciiName
        << "\"; filename*=UTF-8''" << rfc5987 << "\r\n"
        << "Content-Length: " << sz.QuadPart << "\r\n"
        << "Connection: close\r\n"
        << "Access-Control-Allow-Origin: *\r\n\r\n";
    std::string hdrStr = hdr.str();
    send(s, hdrStr.data(), (int)hdrStr.size(), 0);

    char chunk[65536];
    DWORD readBytes = 0;
    while (ReadFile(hf, chunk, sizeof(chunk), &readBytes, nullptr) && readBytes > 0) {
        int sent = 0, total = (int)readBytes;
        while (sent < total) {
            int r = send(s, chunk + sent, total - sent, 0);
            if (r <= 0) goto done;
            sent += r;
        }
    }
done:
    CloseHandle(hf);
}

// ────────────────────────────────────────────────────────────────
//  MULTIPART PARSER
// ────────────────────────────────────────────────────────────────

struct UploadedFile { std::string name; uint64_t size = 0; std::string ts; std::string savedPath; };

static std::vector<UploadedFile> handleUpload(const std::string& reqHeaders,
                                               const std::vector<uint8_t>& body,
                                               const std::string& savingDir,
                                               const std::string& clientIP) {
    std::vector<UploadedFile> results;
    std::string ct = getHeaderVal(reqHeaders, "Content-Type");
    size_t bpos = ct.find("boundary=");
    if (bpos == std::string::npos) { Log(L_ERR, "No boundary in Content-Type"); return results; }
    std::string boundary = "--" + trim(ct.substr(bpos + 9));
    std::string endBound = boundary + "--";
    std::string metadataJson;

    size_t pos = 0;
    while (true) {
        size_t dpos = findSeq(body, pos, boundary);
        if (dpos == std::string::npos) break;
        dpos += boundary.size();
        if (dpos + 2 <= body.size() && body[dpos] == '\r' && body[dpos+1] == '\n') dpos += 2;
        else if (dpos < body.size() && body[dpos] == '-') break;

        std::string hdrsep = "\r\n\r\n";
        size_t hend = findSeq(body, dpos, hdrsep);
        if (hend == std::string::npos) break;
        std::string partHdrs(body.begin() + dpos, body.begin() + hend);
        size_t contentStart = hend + 4;

        size_t nextBound = findSeq(body, contentStart, "\r\n" + boundary);
        if (nextBound == std::string::npos) {
            nextBound = findSeq(body, contentStart, "\r\n" + endBound);
            if (nextBound == std::string::npos) break;
        }
        size_t contentEnd = nextBound;

        std::string cd = getHeaderVal(partHdrs, "Content-Disposition");
        std::string filename = extractFilename(cd);

        if (filename.empty()) {
            if (cd.find("name=\"metadata\"") != std::string::npos) {
                metadataJson.assign(body.begin() + contentStart, body.begin() + contentEnd);
            }
        } else {
            std::string safe = sanitizeFilename(filename);
            std::string outPath = uniquePath(savingDir, safe);
            uint64_t sz = contentEnd - contentStart;

            // ── Storage space check ──
            // If a cap is set: reject if db_used + this file would exceed it.
            // Otherwise:       reject if this file alone exceeds the disk headroom allowed.
            {
                uint64_t cap = g_storageLimitBytes.load();
                if (cap > 0) {
                    uint64_t dbUsed = computeDbUsedBytes();
                    if (dbUsed + sz > cap) {
                        Log(L_ERR, "  REJECTED " + safe + " (" + formatBytes(sz) +
                            "): would exceed storage cap of " + formatBytes(cap) +
                            " (currently used: " + formatBytes(dbUsed) + ")");
                        pos = nextBound + 2;
                        continue;
                    }
                } else {
                    uint64_t maxAllowed = getEffectiveUploadLimitBytes();
                    if (maxAllowed > 0 && sz > maxAllowed) {
                        Log(L_ERR, "  REJECTED " + safe + " (" + formatBytes(sz) +
                            "): exceeds available headroom of " + formatBytes(maxAllowed));
                        pos = nextBound + 2;
                        continue;
                    }
                }
            }

            Log(L_NET, "  Saving: " + safe + " (" + formatBytes(sz) + ")");

            // Use CreateFileW so UTF-8 encoded filename (fullwidth chars) is stored correctly
            HANDLE hf = CreateFileW(toWide(outPath).c_str(), GENERIC_WRITE, 0, nullptr,
                                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf == INVALID_HANDLE_VALUE) {
                Log(L_ERR, "  Cannot create file: " + outPath);
            } else {
                const size_t CHUNK = 4 * 1024 * 1024;
                size_t off = 0;
                while (off < sz) {
                    size_t toWrite = std::min(CHUNK, sz - off);
                    DWORD w = 0;
                    WriteFile(hf, body.data() + contentStart + off, (DWORD)toWrite, &w, nullptr);
                    off += w;
                }
                CloseHandle(hf);

                if (!metadataJson.empty()) {
                    int64_t lm = extractJsonInt64(metadataJson, filename, "lastModified");
                    if (lm > 0) setFileTimes(outPath, lm);
                }

                g_totalBytes += sz;
                g_fileCount++;

                UploadedFile uf;
                uf.name      = filename; // original name — returned to client for upload card display
                uf.size      = sz;
                uf.ts        = nowHuman();
                uf.savedPath = outPath;
                results.push_back(uf);

                // Add to in-memory file list (use original name)
                { std::lock_guard<std::mutex> lk(g_filesMtx);
                  FileRecord fr; fr.name=filename; fr.savedPath=outPath;
                  fr.size=sz; fr.timestamp=uf.ts; fr.from=clientIP;
                  g_files.push_back(fr); }

                // Add to database — name matches the actual filename on disk (encoded)
                DbEntry de;
                de.id        = generateId();
                de.name      = safe;     // encoded name, same as the file on disk
                de.savedPath = outPath;
                de.size      = sz;
                de.timestamp = uf.ts;
                de.from      = clientIP;
                dbAddEntry(de);

                Log(L_OK, "  Saved: " + outPath);
            }
        }
        pos = nextBound + 2;
    }
    return results;
}

// ────────────────────────────────────────────────────────────────
//  CLIENT HANDLER
// ────────────────────────────────────────────────────────────────

static void handleClient(SOCKET s, std::string clientIP) {
    ++g_activeClients;

    // Log connect only when this is the first connection from this IP
    {
        std::lock_guard<std::mutex> lk(g_ipMtx);
        if (++g_ipCount[clientIP] == 1)
            Log(L_VERB, "Device connected: " + clientIP);
    }

    DWORD tv = 30000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));

    std::string hdrs;
    std::vector<uint8_t> body;

    if (!recvAll(s, hdrs, body)) {
        // Silent on poll failures
        closesocket(s);
        --g_activeClients;
        std::lock_guard<std::mutex> lk(g_ipMtx);
        if (--g_ipCount[clientIP] == 0) { g_ipCount.erase(clientIP); Log(L_VERB, "Device disconnected: " + clientIP); }
        return;
    }

    std::string firstLine = hdrs.substr(0, hdrs.find('\n'));
    std::string method, path;
    { std::istringstream ss(firstLine); ss >> method >> path; }

    // Strip query string for route matching
    std::string route = path;
    size_t qpos = route.find('?');
    if (qpos != std::string::npos) route = route.substr(0, qpos);

    Log(L_VERB, clientIP + "  " + method + " " + path);

    std::string savingDir = getSavingDir();

    if (route == "/" || route == "/index.html") {
        sendResp(s, 200, "text/html; charset=utf-8", HTML_PAGE);

    } else if (route == "/api/info") {
        auto ips = getLocalIPs();
        std::string json = "{\"ips\":[";
        for (size_t i = 0; i < ips.size(); ++i) {
            json += "\"" + ips[i] + "\"";
            if (i + 1 < ips.size()) json += ",";
        }
        json += "],\"saving_dir\":\"" + jsonEscape(savingDir) + "\"}";
        sendResp(s, 200, "application/json", json);

    } else if (route == "/api/stats") {
        char buf[256];
        sprintf_s(buf, "{\"files\":%llu,\"bytes\":%llu,\"clients\":%d}",
            g_fileCount.load(), g_totalBytes.load(), g_activeClients.load() - 1);
        sendResp(s, 200, "application/json", std::string(buf));

    } else if (route == "/api/disk_space") {
        std::string dbPath = getDbPath();
        uint64_t freeBytes  = getDiskFreeBytes(dbPath);
        uint64_t totalBytes = getDiskTotalBytes(dbPath);
        uint64_t capBytes   = g_storageLimitBytes.load();
        uint64_t pct        = g_uploadLimitPct.load();
        uint64_t maxUpload  = getEffectiveUploadLimitBytes();
        uint64_t dbUsed     = computeDbUsedBytes();   // sum of database file sizes
        std::string root    = getDiskRoot(dbPath);
        char buf2[512];
        sprintf_s(buf2,
            "{\"disk_free\":%llu,\"disk_total\":%llu,\"storage_cap\":%llu,"
            "\"upload_limit_pct\":%llu,\"max_upload\":%llu,\"disk_root\":\"%s\","
            "\"db_used\":%llu}",
            freeBytes, totalBytes, capBytes, pct, maxUpload, jsonEscape(root).c_str(), dbUsed);
        sendResp(s, 200, "application/json", std::string(buf2));

    } else if (route == "/events") {
        // Server-Sent Events — keep connection open
        std::string hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/event-stream\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n";
        send(s, hdr.data(), (int)hdr.size(), 0);

        // Register this client
        { std::lock_guard<std::mutex> lk(g_sseMtx); g_sseClients.push_back(s); }
        Log(L_VERB, "SSE client registered: " + clientIP);

        // Send initial state
        {
            std::string dbJson = buildDbJson();
            std::string dbMsg = "event: db_update\ndata: " + dbJson + "\n\n";
            send(s, dbMsg.data(), (int)dbMsg.size(), 0);
        }
        {
            std::string pc;
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); pc = g_pastebin; }
            std::string pMsg = "event: pastebin_update\ndata: {\"content\":\"" + jsonEscape(pc) + "\"}\n\n";
            send(s, pMsg.data(), (int)pMsg.size(), 0);
        }

        // Keep thread alive; the socket is now in g_sseClients.
        // Block here by trying to recv (will return when client disconnects or times out)
        // Set a longer recv timeout for SSE clients
        DWORD sseTimeout = 120000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&sseTimeout, sizeof(sseTimeout));
        char dummy[8];
        while (g_running) {
            int r = recv(s, dummy, sizeof(dummy), 0);
            if (r <= 0) break;
        }
        // Remove from SSE clients
        { std::lock_guard<std::mutex> lk(g_sseMtx);
          g_sseClients.erase(std::remove(g_sseClients.begin(), g_sseClients.end(), s), g_sseClients.end()); }
        Log(L_VERB, "SSE client disconnected: " + clientIP);
        closesocket(s);
        --g_activeClients;
        {
            std::lock_guard<std::mutex> lk(g_ipMtx);
            if (--g_ipCount[clientIP] == 0) { g_ipCount.erase(clientIP); Log(L_VERB, "Device disconnected: " + clientIP); }
        }
        return;

    } else if (route == "/api/pastebin") {
        if (method == "GET") {
            std::string content;
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); content = g_pastebin; }
            sendResp(s, 200, "application/json",
                     "{\"content\":\"" + jsonEscape(content) + "\"}");
        } else if (method == "POST") {
            std::string bodyStr(body.begin(), body.end());
            std::string newContent = jsGetStr(bodyStr, "content");
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); g_pastebin = newContent; }
            saveConfigPastebin(newContent);
            sseBroadcastPastebin();
            sendResp(s, 200, "application/json", "{\"success\":true}");
        } else {
            sendResp(s, 400, "text/plain", "Method not allowed");
        }

    } else if (route == "/api/database") {
        sendResp(s, 200, "application/json", buildDbJson());

    } else if (route == "/api/database/delete" && method == "POST") {
        std::string bodyStr(body.begin(), body.end());
        std::string id = jsGetStr(bodyStr, "id");
        bool deleteFile = true;
        {
            size_t dfp = bodyStr.find("\"delete_file\":");
            if (dfp != std::string::npos) {
                std::string rest = bodyStr.substr(dfp + 14);
                rest = trim(rest);
                if (rest.rfind("false",0)==0 || rest.rfind("0",0)==0) deleteFile = false;
            }
        }
        if (id.empty()) {
            sendResp(s, 400, "application/json", "{\"success\":false,\"error\":\"No id provided\"}");
        } else {
            std::string pathToDelete;
            {
                std::lock_guard<std::mutex> lk(g_dbMtx);
                auto it = std::find_if(g_database.begin(), g_database.end(),
                    [&](const DbEntry& e){ return e.id == id; });
                if (it != g_database.end()) {
                    pathToDelete = it->savedPath;
                    g_database.erase(it);
                }
            }
            if (!pathToDelete.empty()) {
                dbSave();
                if (deleteFile) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, pathToDelete.c_str(), -1, nullptr, 0);
                    std::wstring wp(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, pathToDelete.c_str(), -1, &wp[0], wlen);
                    DeleteFileW(wp.c_str());
                    Log(L_OK, "Deleted: " + pathToDelete);
                }
                sseBroadcastDb();
                sendResp(s, 200, "application/json", "{\"success\":true}");
            } else {
                sendResp(s, 404, "application/json", "{\"success\":false,\"error\":\"ID not found\"}");
            }
        }

    } else if (route == "/download") {
        std::string id = queryParam(path, "id");
        if (id.empty()) { sendResp(s, 400, "text/plain", "Missing id"); }
        else {
            std::string filePath, encodedName;
            {
                std::lock_guard<std::mutex> lk(g_dbMtx);
                for (auto& e : g_database) {
                    if (e.id == id) { filePath = e.savedPath; encodedName = e.name; break; }
                }
            }
            if (filePath.empty()) { sendResp(s, 404, "text/plain", "File not found in database"); }
            else {
                // Pass the encoded disk name — Content-Disposition carries the disk name.
                // The client's download= attribute (set to the decoded original) takes
                // precedence for UI-triggered same-origin downloads.
                Log(L_NET, "Download: " + decodeFilenameFromDisk(encodedName) + " → " + clientIP);
                sendFileDownload(s, filePath, encodedName);
            }
        }

    } else if (route == "/download_ff") {
        // Download a file from a forwarding folder by path
        std::string encodedPath = queryParam(path, "path");
        if (encodedPath.empty()) { sendResp(s, 400, "text/plain", "Missing path"); }
        else {
            // Verify it's actually in a forwarding folder
            bool allowed = false;
            {
                std::lock_guard<std::mutex> lk(g_ffMtx);
                for (auto& ff : g_ffFolders) {
                    for (auto& fp : ff.contents) {
                        if (fp == encodedPath) { allowed = true; break; }
                    }
                    if (allowed) break;
                }
            }
            if (!allowed) { sendResp(s, 403, "text/plain", "File not in any forwarding folder"); }
            else {
                // Extract basename (encoded on disk); pass to sendFileDownload as-is.
                // The client's download= attribute carries the decoded original name.
                size_t sl = encodedPath.rfind('\\');
                if (sl == std::string::npos) sl = encodedPath.rfind('/');
                std::string encodedBasename = (sl != std::string::npos) ? encodedPath.substr(sl+1) : encodedPath;
                Log(L_NET, "FF Download: " + decodeFilenameFromDisk(encodedBasename) + " → " + clientIP);
                sendFileDownload(s, encodedPath, encodedBasename);
            }
        }

    } else if (route == "/upload" && method == "POST") {
        Log(L_INFO, "Upload from " + clientIP + " (" + formatBytes(body.size()) + ")");
        auto uploaded = handleUpload(hdrs, body, savingDir, clientIP);
        if (uploaded.empty()) {
            sendResp(s, 400, "application/json", "{\"success\":false,\"error\":\"No files parsed\"}");
        } else {
            std::string json = "{\"success\":true,\"saving_dir\":\"" + jsonEscape(savingDir) + "\",\"files\":[";
            for (size_t i = 0; i < uploaded.size(); ++i) {
                auto& f = uploaded[i];
                json += "{\"name\":\"" + jsonEscape(f.name) + "\",\"size\":" +
                        std::to_string(f.size) + ",\"ts\":\"" + f.ts + "\"}";
                if (i + 1 < uploaded.size()) json += ",";
            }
            json += "]}";
            sendResp(s, 200, "application/json", json);
            Log(L_OK, std::to_string(uploaded.size()) + " file(s) saved from " + clientIP);
            sseBroadcastDb();
        }
    } else {
        sendResp(s, 404, "text/plain", "404 Not Found");
    }

    closesocket(s);
    --g_activeClients;
    {
        std::lock_guard<std::mutex> lk(g_ipMtx);
        if (--g_ipCount[clientIP] == 0) { g_ipCount.erase(clientIP); Log(L_VERB, "Device disconnected: " + clientIP); }
    }
}

// ────────────────────────────────────────────────────────────────
//  PORT DETECTION + SERVER
// ────────────────────────────────────────────────────────────────

static bool portAvailable(int port) {
    SOCKET ts = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ts == INVALID_SOCKET) return false;
    BOOL reuse = TRUE;
    setsockopt(ts, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);
    int r = bind(ts, (sockaddr*)&addr, sizeof(addr));
    closesocket(ts);
    return r == 0;
}

static int findOpenPort() {
    std::vector<int> preferred = {8080, 8081, 8082, 8083, 8084, 8085, 9090, 9091, 7070, 7777, 3000, 4000, 5000};
    for (int p : preferred) {
        if (portAvailable(p)) { Log(L_OK, "Port " + std::to_string(p) + " is available"); return p; }
        Log(L_VERB, "Port " + std::to_string(p) + " in use, trying next...");
    }
    for (int p = 49152; p < 65535; ++p) {
        if (portAvailable(p)) { Log(L_WARN, "Using fallback port " + std::to_string(p)); return p; }
    }
    return -1;
}

static void serverThread(int port) {
    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) { Log(L_ERR, "socket() failed"); g_running = false; return; }

    BOOL reuse = TRUE;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    int sndbuf = 256*1024, rcvbuf = 256*1024;
    setsockopt(srv, SOL_SOCKET, SO_SNDBUF, (char*)&sndbuf, sizeof(sndbuf));
    setsockopt(srv, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((u_short)port);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0) {
        Log(L_ERR, "bind() failed: " + std::to_string(WSAGetLastError()));
        closesocket(srv); g_running = false; return;
    }
    if (listen(srv, SOMAXCONN) != 0) {
        Log(L_ERR, "listen() failed");
        closesocket(srv); g_running = false; return;
    }
    Log(L_OK, "Server listening on 0.0.0.0:" + std::to_string(port));

    // NOTE: Do NOT set SO_RCVTIMEO here — that affects recv(), not accept().
    // We use select() with a 1-second timeout before each accept() so that
    // the loop checks g_running every second and exits cleanly on /quit.

    while (g_running) {
        // Wait up to 1 second for an incoming connection
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(srv, &readfds);
        struct timeval tv = {1, 0};
        int sel = select(0, &readfds, nullptr, nullptr, &tv);
        if (sel == 0) continue;                   // timeout — re-check g_running
        if (sel == SOCKET_ERROR) {
            if (g_running) Log(L_WARN, "select() error " + std::to_string(WSAGetLastError()));
            continue;
        }

        sockaddr_in ca{};
        int al = sizeof(ca);
        SOCKET cs = accept(srv, (sockaddr*)&ca, &al);
        if (cs == INVALID_SOCKET) {
            int err = WSAGetLastError();
            if (g_running) Log(L_WARN, "accept() error " + std::to_string(err));
            continue;
        }
        char ip[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof(ip));
        std::thread(handleClient, cs, std::string(ip)).detach();
    }
    closesocket(srv);
    Log(L_INFO, "Server thread stopped.");
}

// ────────────────────────────────────────────────────────────────
//  INPUT  (char-by-char, redraws prompt on log interruption)
// ────────────────────────────────────────────────────────────────

static std::string readLine() {
    { std::lock_guard<std::mutex> lk(g_logMtx); g_inputBuf.clear(); }
    g_inputActive = true;

    while (g_running) {
        int c = _getch();
        if (c == '\r' || c == '\n') {
            g_inputActive = false;
            std::string result;
            { std::lock_guard<std::mutex> lk(g_logMtx);
              std::cout << "\n"; result = g_inputBuf; }
            return result;
        } else if (c == 8) { // backspace
            std::lock_guard<std::mutex> lk(g_logMtx);
            if (!g_inputBuf.empty()) {
                g_inputBuf.pop_back();
                std::cout << "\b \b";
                std::cout.flush();
            }
        } else if (c == 3) { // Ctrl+C
            g_inputActive = false;
            g_running = false;
            return "/exit";
        } else if (c == 0 || c == 224) {
            _getch(); // consume extended key
        } else if (c >= 32 && c < 127) {
            std::lock_guard<std::mutex> lk(g_logMtx);
            g_inputBuf += (char)c;
            std::cout << (char)c;
            std::cout.flush();
        }
    }
    g_inputActive = false;
    return "/exit";
}

// ────────────────────────────────────────────────────────────────
//  COMMAND PROCESSOR
// ────────────────────────────────────────────────────────────────

static void printHelp() {
    SetConsoleTextAttribute(g_hCon, 3);  // dark cyan — readable on black, not harsh
    std::cout << "\n  ┌──────────────────────────────────────────────────────────────────────┐\n";
    std::cout << "  │              localTransfer.io  –  Command Reference                 │\n";
    std::cout << "  │          Tip: use / or \\ as command prefix interchangeably           │\n";
    std::cout << "  ├──────────────────────────────────────────┬───────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << "  │ /help                                │ Show this help                │\n";
    std::cout << "  │ /verbose  /verbose on|off            │ Toggle verbose logging        │\n";
    std::cout << "  │ /verbose <level> on|off              │ Mute/unmute specific level    │\n";
    std::cout << "  │ /status                              │ Server status & statistics    │\n";
    std::cout << "  │ /files                               │ List all received files       │\n";
    std::cout << "  │ /port  /ips                          │ Show port / network addresses │\n";
    std::cout << "  │ /saving_dir_change <path>  /sdc      │ Change saving directory       │\n";
    SetConsoleTextAttribute(g_hCon, 3);
    std::cout << "  ├──────────────────────────────────────┼───────────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << "  │ /disk_space  /ds                     │ Show disk & upload limit info │\n";
    std::cout << "  │ /storage_limit <size|off>  /sl       │ Set app storage cap           │\n";
    std::cout << "  │   e.g. /sl 100gb  /sl 500mb  /sl off│ (80% of cap = upload limit)   │\n";
    SetConsoleTextAttribute(g_hCon, 3);
    std::cout << "  ├──────────────────────────────────────┼───────────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << "  │ /database --list   / -l              │ List database entries         │\n";
    std::cout << "  │ /database --reload / -r              │ Reload database from disk     │\n";
    std::cout << "  │ /database --open   / -o              │ Open database.json in editor  │\n";
    std::cout << "  │ /database --clear  / -c              │ Clear entries (keep files)    │\n";
    std::cout << "  │ /database --delete-files / -df       │ Clear + delete files on disk  │\n";
    std::cout << "  │ /database --push <path>              │ Push a host file to database  │\n";
    std::cout << "  │ /db                                  │ Alias for /database           │\n";
    SetConsoleTextAttribute(g_hCon, 3);
    std::cout << "  ├──────────────────────────────────────┼───────────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << "  │ /ff --new <path>  /forwarding_folder │ Add a forwarding folder       │\n";
    std::cout << "  │ /ff --remove <id>                    │ Remove a forwarding folder    │\n";
    std::cout << "  │ /ff --list                           │ List forwarding folders       │\n";
    SetConsoleTextAttribute(g_hCon, 3);
    std::cout << "  ├──────────────────────────────────────┼───────────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << "  │ /pastebin --clear                    │ Clear pastebin content        │\n";
    std::cout << "  │ /pastebin --overwrite \"text\"         │ Replace pastebin content      │\n";
    std::cout << "  │ /pastebin --append \"text\"            │ Append text (no separator)    │\n";
    std::cout << "  │ /pastebin --append-nl \"text\"         │ Append with 2 newlines        │\n";
    std::cout << "  │ /pastebin --copy  /pb                │ Copy to clipboard / alias     │\n";
    SetConsoleTextAttribute(g_hCon, 3);
    std::cout << "  ├──────────────────────────────────────┼───────────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 7);
    std::cout << "  │ /clear  /clear --absolute            │ Clear terminal / no banner    │\n";
    std::cout << "  │ /exit  or  /quit                     │ Stop server and exit          │\n";
    SetConsoleTextAttribute(g_hCon, 3);
    std::cout << "  └──────────────────────────────────────┴───────────────────────────────┘\n\n";
    SetConsoleTextAttribute(g_hCon, 7);
}

static bool isTrue(const std::string& s) {
    return s == "1" || s == "true" || s == "yes" || s == "on";
}
static bool isFalse(const std::string& s) {
    return s == "0" || s == "false" || s == "no" || s == "off";
}

// ────────────────────────────────────────────────────────────────
//  BANNER
// ────────────────────────────────────────────────────────────────

// Prints the banner art without clearing (used by /clear)
static void printBannerArt(int port, const std::vector<std::string>& ips) {
    // Warm gold gradient wordmark banner
    // Windows console colors: 8=dark grey, 6=dark yellow/gold, 14=bright yellow, 7=light grey
    std::cout << "\n";

    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "  ══════════════════════════════════════════════════════════════\n";

    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "\n";
    std::cout << "    ██╗      ██████╗  ██████╗ █████╗ ██╗\n";

    SetConsoleTextAttribute(g_hCon, 10);
    std::cout << "    ██║     ██╔═══██╗██╔════╝██╔══██╗██║\n";
    std::cout << "    ██║     ██║   ██║██║     ███████║██║\n";

    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "    ██║     ██║   ██║██║     ██╔══██║██║\n";
    std::cout << "    ███████╗╚██████╔╝╚██████╗██║  ██║███████╗\n";

    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "    ╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝╚══════╝\n";
    std::cout << "\n";

    SetConsoleTextAttribute(g_hCon, 10);
    std::cout << "    ████████╗██████╗  █████╗ ███╗  ██╗███████╗███████╗███████╗██████╗\n";
    std::cout << "       ██║   ██╔══██╗██╔══██╗████╗ ██║██╔════╝██╔════╝██╔════╝██╔══██╗\n";

    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "       ██║   ██████╔╝███████║██╔██╗██║███████╗█████╗  █████╗  ██████╔╝\n";
    std::cout << "       ██║   ██╔══██╗██╔══██║██║╚████║╚════██║██╔══╝  ██╔══╝  ██╔══██╗\n";

    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "       ██║   ██║  ██║██║  ██║██║ ╚███║███████║██║     ███████╗██║  ██║\n";
    std::cout << "       ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚══╝╚══════╝╚═╝     ╚══════╝╚═╝  ╚═╝\n";

    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "\n";
    std::cout << "                            ";
    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "·-·-·| ";
    SetConsoleTextAttribute(g_hCon, 10);
    std::cout << "localTransfer.io";
    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << " |·-·-·\n";

    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "              local network file transfer — no internet needed\n";
    std::cout << "              Architected by: Nicanor III W. Cariasa (2026)\n";
    std::cout << "\n";
    std::cout << "  ══════════════════════════════════════════════════════════════\n\n";

    std::string savingDir = getSavingDir();
    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "  ┌─────────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Open in any browser on this network:                       │\n";
    for (auto& ip : ips) {
        std::string url = "    http://" + ip + ":" + std::to_string(port) + "/";
        SetConsoleTextAttribute(g_hCon, 10);
        std::cout << "  │  " << std::left << std::setw(57) << url << "│\n";
    }
    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "  ├─────────────────────────────────────────────────────────────┤\n";
    SetConsoleTextAttribute(g_hCon, 8);
    std::cout << "  │  Saving to: " << std::left << std::setw(48) << savingDir << "│\n";
    std::cout << "  │  Database:  " << std::left << std::setw(48) << getDbPath() << "│\n";
    {
        uint64_t maxUp = getEffectiveUploadLimitBytes();
        std::string limStr = (maxUp > 0) ? ("Upload limit: " + formatBytes(maxUp)) : "Upload limit: (no disk space)";
        std::cout << "  │  " << std::left << std::setw(59) << limStr << "│\n";
    }
    SetConsoleTextAttribute(g_hCon, 2);
    std::cout << "  │  Type /help for commands  (/ or \\ prefix both work)        │\n";
    std::cout << "  └─────────────────────────────────────────────────────────────┘\n\n";
    SetConsoleTextAttribute(g_hCon, 7);
}

static void processCommand(const std::string& raw) {
    std::string cmd = trim(raw);
    if (cmd.empty()) return;

    // Support both / and \ as command prefix
    if (!cmd.empty() && cmd[0] == '\\') cmd[0] = '/';

    std::string lo = cmd;
    std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);

    Log(L_CMD, "> " + cmd);

    if (lo == "/help") {
        printHelp();

    } else if (lo.rfind("/verbose", 0) == 0) {
        std::string arg = trim(lo.substr(8));

        // Map level names to LogLvl enum
        static const std::map<std::string, int> lvlMap = {
            {"info",0}, {"ok",1}, {"warn",2}, {"err",3},
            {"verb",4}, {"verbose",4}, {"cmd",5}, {"net",6}
        };

        if (arg.empty()) {
            // Toggle all verbose (VERB messages on/off)
            g_verbose = !g_verbose;
            Log(L_INFO, std::string("Verbose ") + (g_verbose ? "ON" : "OFF"));
        } else if (isTrue(arg)) {
            g_verbose = true;
            Log(L_OK, "Verbose logging ENABLED");
        } else if (isFalse(arg)) {
            g_verbose = false;
            Log(L_OK, "Verbose logging DISABLED");
        } else {
            // Check for named-level toggle: /verbose net off, /verbose cmd 0, /verbose info on
            // Format: /verbose <level> [on|off|0|1|toggle]
            std::istringstream iss(arg);
            std::string lvlName, onoff;
            iss >> lvlName >> onoff;
            if (lvlMap.count(lvlName)) {
                int bit = lvlMap.at(lvlName);
                uint32_t mask = 1u << bit;
                uint32_t cur = g_mutedLevels.load();
                bool muted;
                if (onoff.empty() || onoff == "toggle") {
                    muted = !(cur & mask); // toggle
                } else {
                    muted = isFalse(onoff); // off=mute, on=unmute
                }
                if (muted) {
                    g_mutedLevels.fetch_or(mask);
                    Log(L_INFO, "[" + lvlName + "] messages MUTED");
                } else {
                    g_mutedLevels.fetch_and(~mask);
                    Log(L_INFO, "[" + lvlName + "] messages VISIBLE");
                }
            } else {
                Log(L_WARN, "Usage: /verbose  |  /verbose on/off  |  /verbose <level> [on|off]");
                Log(L_INFO, "Levels: info  ok  warn  err  net  cmd  verb");
            }
        }

    } else if (lo == "/status") {
        SetConsoleTextAttribute(g_hCon, 11);
        std::cout << "\n  ── Server Status ────────────────────────────────\n";
        SetConsoleTextAttribute(g_hCon, 7);
        std::cout << "  Port          : "; SetConsoleTextAttribute(g_hCon,10);
        std::cout << g_port.load();       SetConsoleTextAttribute(g_hCon, 7);
        std::cout << "\n  Active clients: " << g_activeClients.load();
        std::cout << "\n  Files received: " << g_fileCount.load();
        std::cout << "\n  Data received : " << formatBytes(g_totalBytes.load());
        std::cout << "\n  Verbose mode  : " << (g_verbose ? "ON" : "OFF");
        std::cout << "\n  Saving dir    : " << getSavingDir();
        { std::lock_guard<std::mutex> lk(g_dbMtx);
          std::cout << "\n  Database size : " << g_database.size() << " entries"; }
        std::cout << "\n  Database path : " << getDbPath();
        std::cout << "\n  ─────────────────────────────────────────────────\n\n";

    } else if (lo == "/files") {
        std::lock_guard<std::mutex> lk(g_filesMtx);
        if (g_files.empty()) { Log(L_INFO, "No files received yet."); }
        else {
            SetConsoleTextAttribute(g_hCon, 11);
            std::cout << "\n  ── Received Files (" << g_files.size() << ") ────────────────────────\n";
            SetConsoleTextAttribute(g_hCon, 7);
            for (size_t i = 0; i < g_files.size(); ++i) {
                auto& f = g_files[i];
                SetConsoleTextAttribute(g_hCon, 10);
                std::cout << "  [" << (i+1) << "] ";
                SetConsoleTextAttribute(g_hCon, 15);
                std::cout << f.name;
                SetConsoleTextAttribute(g_hCon, 8);
                std::cout << "  (" << formatBytes(f.size) << ")  " << f.timestamp << "\n";
            }
            SetConsoleTextAttribute(g_hCon, 7);
            std::cout << "\n";
        }

    } else if (lo == "/port") {
        Log(L_INFO, "Active port: " + std::to_string(g_port.load()));

    } else if (lo == "/ips") {
        auto ips = getLocalIPs();
        Log(L_INFO, "Network access URLs:");
        for (auto& ip : ips) {
            SetConsoleTextAttribute(g_hCon, 11);
            std::cout << "    http://" << ip << ":" << g_port.load() << "/\n";
        }
        SetConsoleTextAttribute(g_hCon, 7);

    } else if (lo.rfind("/saving_dir_change", 0) == 0 || lo.rfind("/sdc", 0) == 0) {
        std::string newDir;

		if (lo.rfind("/saving_dir_change", 0) == 0) {
			newDir = trim(cmd.substr(18)); // "/saving_dir_change" = 18 chars
		} else {
			newDir = trim(cmd.substr(4));  // "/sdc" = 4 chars
		}

        if (newDir.empty()) {
            Log(L_WARN, "Usage: /saving_dir_change <path>");
        } else {
			
			// Strip surrounding quotes if present
            if (newDir.size() >= 2 && newDir.front() == '"' && newDir.back() == '"')
                newDir = newDir.substr(1, newDir.size() - 2);
            else if (newDir.size() >= 2 && newDir.front() == '\'' && newDir.back() == '\'')
                newDir = newDir.substr(1, newDir.size() - 2);
			
            bool ok = false;
            DWORD attrs = GetFileAttributesA(newDir.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES) {
                if (CreateDirectoryA(newDir.c_str(), nullptr)) ok = true;
                else Log(L_ERR, "Cannot create directory: " + newDir);
            } else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                ok = true;
            } else {
                Log(L_ERR, "Path is not a directory: " + newDir);
            }
            if (ok) {
                setSavingDir(newDir);
                Log(L_OK, "Saving directory changed to: " + newDir);
                Log(L_INFO, "(Existing database entries are unaffected)");
            }
        }

    } else if (lo.rfind("/database", 0) == 0 || lo.rfind("/db", 0) == 0) {
        // Parse subcommand
        std::string rest;
        if (lo.rfind("/database", 0) == 0) rest = trim(lo.substr(9));
        else rest = trim(lo.substr(3));

        if (rest == "--list" || rest == "-l") {
            std::lock_guard<std::mutex> lk(g_dbMtx);
            if (g_database.empty()) { Log(L_INFO, "Database is empty."); }
            else {
                SetConsoleTextAttribute(g_hCon, 11);
                std::cout << "\n  ── Database (" << g_database.size() << " entries) ──────────────────────────────\n";
                SetConsoleTextAttribute(g_hCon, 7);
                for (size_t i = 0; i < g_database.size(); ++i) {
                    auto& e = g_database[i];
                    SetConsoleTextAttribute(g_hCon, 10);
                    std::cout << "  [" << (i+1) << "] ";
                    SetConsoleTextAttribute(g_hCon, 15);
                    std::cout << e.name;
                    SetConsoleTextAttribute(g_hCon, 8);
                    std::cout << "  (" << formatBytes(e.size) << ")  " << e.timestamp
                              << "  from:" << e.from << "\n";
                    SetConsoleTextAttribute(g_hCon, 3);
                    std::cout << "       " << e.savedPath << "\n";
                }
                SetConsoleTextAttribute(g_hCon, 7);
                std::cout << "\n";
            }

        } else if (rest == "--reload" || rest == "-r") {
            dbLoad();
            dbUpdateCheck();
            dbSave();
            { std::lock_guard<std::mutex> lk(g_dbMtx);
              Log(L_OK, "Database reloaded: " + std::to_string(g_database.size()) + " entries"); }

        } else if (rest == "--open" || rest == "-o") {
            std::string dbp = getDbPath();
            // Ensure file exists
            { std::ofstream tmp(dbp, std::ios::app); }
            ShellExecuteA(nullptr, "open", dbp.c_str(), nullptr, nullptr, SW_SHOW);
            Log(L_INFO, "Opened: " + dbp);

        } else if (rest == "--clear" || rest == "-c") {
            { std::lock_guard<std::mutex> lk(g_dbMtx); g_database.clear(); }
            dbSave();
            Log(L_OK, "Database cleared (files on disk NOT deleted)");

        } else if (rest.rfind("--delete-files", 0) == 0 || rest.rfind("-df", 0) == 0) {
            std::string arg;
            if (rest.rfind("--delete-files", 0) == 0) arg = trim(rest.substr(14));
            else arg = trim(rest.substr(3));
            bool doDelete = arg.empty() || isTrue(arg); // default: delete files too

            std::vector<std::string> paths;
            { std::lock_guard<std::mutex> lk(g_dbMtx);
              for (auto& e : g_database) paths.push_back(e.savedPath);
              g_database.clear(); }
            dbSave();

            if (doDelete) {
                int deleted = 0;
                for (auto& p : paths) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, nullptr, 0);
                    std::wstring wp(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, &wp[0], wlen);
                    if (DeleteFileW(wp.c_str())) ++deleted;
                }
                Log(L_OK, "Database cleared. " + std::to_string(deleted) + "/" +
                    std::to_string(paths.size()) + " files deleted from disk.");
            } else {
                Log(L_OK, "Database cleared. Files on disk NOT deleted (" +
                    std::to_string(paths.size()) + " files kept).");
            }

        } else if (rest.rfind("--push", 0) == 0 || rest.rfind("-p ", 0) == 0 || rest == "-p") {
            // Extract raw path from original cmd (preserve casing and backslashes)
            int prefixLen = (lo.rfind("/database",0)==0) ? 9 : 3;
            std::string dbRaw = trim(cmd.substr(prefixLen)); // everything after /database or /db
            int subLen = (dbRaw.rfind("--push",0)==0) ? 6 : 2;
            std::string filePath = trim(dbRaw.substr(subLen));

            // Strip surrounding quotes if present
            if (filePath.size() >= 2 && filePath.front() == '"' && filePath.back() == '"')
                filePath = filePath.substr(1, filePath.size() - 2);
            else if (filePath.size() >= 2 && filePath.front() == '\'' && filePath.back() == '\'')
                filePath = filePath.substr(1, filePath.size() - 2);

            if (filePath.empty()) {
                Log(L_WARN, "Usage: /database --push <path>  (quotes optional)");
            } else {
                // Try narrow path first, then wide
                DWORD attrs = GetFileAttributesA(filePath.c_str());
                // Also try wide path in case of Unicode characters
                if (attrs == INVALID_FILE_ATTRIBUTES) {
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
                    std::wstring wp(wlen, 0);
                    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wp[0], wlen);
                    attrs = GetFileAttributesW(wp.c_str());
                }

                if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    Log(L_ERR, "File not found or is a directory: " + filePath);
                } else {
                    // Get file size via wide API for full Unicode support
                    int wlen2 = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
                    std::wstring wp2(wlen2, 0);
                    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wp2[0], wlen2);
                    uint64_t fsz = 0;
                    HANDLE hf = CreateFileW(wp2.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hf != INVALID_HANDLE_VALUE) {
                        LARGE_INTEGER li; GetFileSizeEx(hf, &li); fsz = li.QuadPart;
                        CloseHandle(hf);
                    }
                    // Extract filename from path
                    std::string fname = filePath;
                    size_t sl = fname.rfind('\\');
                    if (sl == std::string::npos) sl = fname.rfind('/');
                    if (sl != std::string::npos) fname = fname.substr(sl + 1);

                    DbEntry de;
                    de.id        = generateId();
                    de.name      = fname;
                    de.savedPath = filePath;
                    de.size      = fsz;
                    de.timestamp = nowHuman();
                    de.from      = "host";
                    dbAddEntry(de);
                    sseBroadcastDb();
                    Log(L_OK, "Pushed to database: " + fname + " (" + formatBytes(fsz) + ")");
                }
            }

        } else {
            Log(L_INFO, "Database commands: --list/-l  --reload/-r  --open/-o  --clear/-c  --delete-files/-df  --push <path>");
        }

    } else if (lo == "/disk_space" || lo == "/ds") {
        std::string dbPath = getDbPath();
        std::string root   = getDiskRoot(dbPath);
        uint64_t freeB     = getDiskFreeBytes(dbPath);
        uint64_t totalB    = getDiskTotalBytes(dbPath);
        uint64_t capB      = g_storageLimitBytes.load();
        uint64_t maxUpB    = getEffectiveUploadLimitBytes();
        SetConsoleTextAttribute(g_hCon, 11);
        std::cout << "\n  ── Disk / Storage Info ──────────────────────────────\n";
        SetConsoleTextAttribute(g_hCon, 7);
        std::cout << "  Database disk root : " << root << "\n";
        std::cout << "  Disk free          : " << formatBytes(freeB) << "\n";
        std::cout << "  Disk total         : " << formatBytes(totalB) << "\n";
        if (capB > 0)
            std::cout << "  App storage cap    : " << formatBytes(capB) << "\n";
        else
            std::cout << "  App storage cap    : (none — using disk free)\n";
        std::cout << "  Upload limit (" << g_uploadLimitPct.load() << "%) : " << formatBytes(maxUpB) << "\n";
        std::cout << "  ─────────────────────────────────────────────────────\n\n";
        SetConsoleTextAttribute(g_hCon, 7);

    } else if (lo.rfind("/storage_limit", 0) == 0 || lo.rfind("/sl ", 0) == 0 || lo == "/sl") {
        int prefLen = (lo.rfind("/storage_limit",0)==0) ? 14 : 3;
        std::string arg = trim(cmd.substr(prefLen));
        if (arg.size() >= 2 && arg.front()=='"' && arg.back()=='"') arg = arg.substr(1, arg.size()-2);
        if (arg.empty()) {
            uint64_t cap = g_storageLimitBytes.load();
            if (cap == 0) Log(L_INFO, "Storage limit: OFF (using disk free space)");
            else          Log(L_INFO, "Storage limit: " + formatBytes(cap));
        } else if (arg == "off" || arg == "0" || arg == "none") {
            g_storageLimitBytes.store(0);
            saveConfigStorageLimit(0);
            Log(L_OK, "Storage limit removed — using disk free space");
        } else {
            uint64_t bytes = parseHumanSize(arg);
            if (bytes == 0) {
                Log(L_WARN, "Usage: /storage_limit <size>  e.g. 100gb  500mb  2tb  |  off to remove");
            } else {
                g_storageLimitBytes.store(bytes);
                saveConfigStorageLimit(bytes);
                Log(L_OK, "Storage limit set to " + formatBytes(bytes));
                Log(L_INFO, "Max upload per file: " + formatBytes(getEffectiveUploadLimitBytes()));
            }
        }

    } else if (lo.rfind("/forwarding_folder", 0) == 0 || lo.rfind("/ff", 0) == 0) {
        bool isAlias = (lo.rfind("/ff", 0) == 0);
        int prefLen  = isAlias ? 3 : 18;
        std::string rest    = trim(lo.substr(prefLen));
        std::string restRaw = trim(cmd.substr(prefLen));

        auto ffGenId = []() -> std::string {
            static std::mt19937 r(std::random_device{}());
            std::uniform_int_distribution<int> d(0, 35);
            const char* ch = "abcdefghijklmnopqrstuvwxyz0123456789";
            std::string id;
            for (int i = 0; i < 8; ++i) id += ch[d(r)];
            return id;
        };

        if (rest.rfind("--new", 0) == 0 || rest.rfind("-n", 0) == 0) {
            int skip = rest.rfind("--new",0)==0 ? 5 : 2;
            std::string ffPath = trim(restRaw.substr(skip));
            if (ffPath.size() >= 2 && ffPath.front()=='"' && ffPath.back()=='"')
                ffPath = ffPath.substr(1, ffPath.size()-2);
            if (ffPath.empty()) {
                Log(L_WARN, "Usage: /ff --new <path>");
            } else {
                // Create dir if it doesn't exist
                bool ok = false;
                DWORD attrs = GetFileAttributesA(ffPath.c_str());
                if (attrs == INVALID_FILE_ATTRIBUTES) {
                    if (CreateDirectoryA(ffPath.c_str(), nullptr)) ok = true;
                    else Log(L_ERR, "Cannot create directory: " + ffPath);
                } else if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
                    ok = true;
                } else {
                    Log(L_ERR, "Path is not a directory: " + ffPath);
                }
                if (ok) {
                    // Check for duplicate
                    bool dup = false;
                    { std::lock_guard<std::mutex> lk(g_ffMtx);
                      for (auto& f : g_ffFolders) if (f.path == ffPath) { dup = true; break; } }
                    if (dup) {
                        Log(L_WARN, "Already watching: " + ffPath);
                    } else {
                        ForwardingFolder ff;
                        ff.id   = ffGenId();
                        ff.path = ffPath;
                        { std::lock_guard<std::mutex> lk(g_ffMtx); g_ffFolders.push_back(ff); }
                        dbSave();
                        Log(L_OK, "Forwarding folder added [" + ff.id + "]: " + ffPath);
                    }
                }
            }

        } else if (rest.rfind("--remove", 0) == 0 || rest.rfind("-rm", 0) == 0) {
            int skip = rest.rfind("--remove",0)==0 ? 8 : 3;
            std::string ffId = trim(restRaw.substr(skip));
            if (ffId.empty()) {
                Log(L_WARN, "Usage: /ff --remove <id>  (see /ff --list)");
            } else {
                bool found = false;
                { std::lock_guard<std::mutex> lk(g_ffMtx);
                  auto it = std::find_if(g_ffFolders.begin(), g_ffFolders.end(),
                      [&](const ForwardingFolder& f){ return f.id == ffId; });
                  if (it != g_ffFolders.end()) { g_ffFolders.erase(it); found = true; } }
                if (found) {
                    dbSave();
                    sseBroadcastDb();
                    Log(L_OK, "Forwarding folder [" + ffId + "] removed");
                } else {
                    Log(L_ERR, "No forwarding folder with id: " + ffId);
                }
            }

        } else if (rest == "--list" || rest == "-l" || rest.empty()) {
            std::lock_guard<std::mutex> lk(g_ffMtx);
            if (g_ffFolders.empty()) {
                Log(L_INFO, "No forwarding folders configured.");
            } else {
                SetConsoleTextAttribute(g_hCon, 11);
                std::cout << "\n  ── Forwarding Folders (" << g_ffFolders.size() << ") ───────────────────────────\n";
                SetConsoleTextAttribute(g_hCon, 7);
                for (auto& ff : g_ffFolders) {
                    SetConsoleTextAttribute(g_hCon, 14); // light yellow
                    std::cout << "  [";
                    SetConsoleTextAttribute(g_hCon, 10); // green
                    std::cout << "id: ";
                    SetConsoleTextAttribute(g_hCon, 15); // bright white
                    std::cout << ff.id;
                    SetConsoleTextAttribute(g_hCon, 14); // light yellow
                    std::cout << "]  ";
                    SetConsoleTextAttribute(g_hCon, 15);
                    std::cout << ff.path;
                    SetConsoleTextAttribute(g_hCon, 8);
                    std::cout << "  (" << ff.contents.size() << " files)\n";
                    for (auto& fp : ff.contents) {
                        size_t sl = fp.rfind('\\');
                        std::string fn = (sl!=std::string::npos) ? fp.substr(sl+1) : fp;
                        std::cout << "     · " << fn << "\n";
                    }
                }
                SetConsoleTextAttribute(g_hCon, 7);
                std::cout << "\n";
            }
        } else {
            Log(L_INFO, "Forwarding folder commands: --new/-n <path>  --remove/-rm <id>  --list/-l");
        }

    } else if (lo.rfind("/pastebin", 0) == 0 || (lo.rfind("/pb", 0) == 0 && lo.size() <= 3)) {
        // Parse subcommand and argument (preserve case for content args)
        bool isPb = lo.rfind("/pb", 0) == 0 && lo.size() <= 3;
        bool isDb = lo.rfind("/pastebin", 0) == 0;
        int prefixLen = isDb ? 9 : 3;

        std::string restLo = trim(lo.substr(prefixLen));
        std::string restRaw = trim(cmd.substr(prefixLen));

        // Helper to extract quoted or unquoted argument
        auto extractArg = [&](const std::string& raw, int subLen) -> std::string {
            std::string a = trim(raw.substr(subLen));
            if (!a.empty() && a.front() == '"') {
                if (a.size() >= 2 && a.back() == '"') a = a.substr(1, a.size()-2);
                else a = a.substr(1);
            }
            return a;
        };

        if (restLo == "--clear") {
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); g_pastebin.clear(); }
            saveConfigPastebin("");
            sseBroadcastPastebin();
            Log(L_OK, "Pastebin cleared");

        } else if (restLo.rfind("--overwrite", 0) == 0) {
            std::string newContent = extractArg(restRaw, 11);
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); g_pastebin = newContent; }
            saveConfigPastebin(newContent);
            sseBroadcastPastebin();
            Log(L_OK, "Pastebin overwritten (" + std::to_string(newContent.size()) + " chars)");

        } else if (restLo.rfind("--append-nl", 0) == 0 || restLo.rfind("--append-text-newline", 0) == 0) {
            int skip = restLo.rfind("--append-nl",0)==0 ? 11 : 21;
            std::string toAppend = extractArg(restRaw, skip);
            { std::lock_guard<std::mutex> lk(g_pastebinMtx);
              g_pastebin += "\n\n" + toAppend; }
            std::string snap; { std::lock_guard<std::mutex> lk(g_pastebinMtx); snap = g_pastebin; }
            saveConfigPastebin(snap);
            sseBroadcastPastebin();
            Log(L_OK, "Pastebin appended (with newlines)");

        } else if (restLo.rfind("--append", 0) == 0 || restLo.rfind("--append-text ", 0) == 0) {
            int skip = restLo.rfind("--append-text ",0)==0 ? 14 : 8;
            std::string toAppend = extractArg(restRaw, skip);
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); g_pastebin += toAppend; }
            std::string snap; { std::lock_guard<std::mutex> lk(g_pastebinMtx); snap = g_pastebin; }
            saveConfigPastebin(snap);
            sseBroadcastPastebin();
            Log(L_OK, "Pastebin appended");

        } else if (restLo == "--copy") {
            std::string content;
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); content = g_pastebin; }
            if (content.empty()) {
                Log(L_WARN, "Pastebin is empty, nothing to copy");
            } else if (OpenClipboard(nullptr)) {
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, content.size() + 1);
                if (hMem) {
                    memcpy(GlobalLock(hMem), content.data(), content.size() + 1);
                    GlobalUnlock(hMem);
                    EmptyClipboard();
                    SetClipboardData(CF_TEXT, hMem);
                }
                CloseClipboard();
                Log(L_OK, "Pastebin copied to clipboard (" + std::to_string(content.size()) + " chars)");
            } else { Log(L_ERR, "Cannot open clipboard"); }

        } else if (restLo.empty()) {
            // Show current content
            std::string content;
            { std::lock_guard<std::mutex> lk(g_pastebinMtx); content = g_pastebin; }
            SetConsoleTextAttribute(g_hCon, 11);
            std::cout << "\n  ── Pastebin (" << content.size() << " chars) ──────────────────────────\n";
            SetConsoleTextAttribute(g_hCon, 7);
            std::cout << content << "\n";
            SetConsoleTextAttribute(g_hCon, 11);
            std::cout << "  ────────────────────────────────────────────────────────\n\n";
            SetConsoleTextAttribute(g_hCon, 7);

        } else {
            Log(L_INFO, "Pastebin commands: --clear  --overwrite \"\"  --append \"\"  --append-nl \"\"  --copy");
        }

    } else if (lo.rfind("/clear", 0) == 0) {
        std::string clearArg = trim(lo.substr(6));
        system("cls");
        if (clearArg != "--absolute" && clearArg != "-a") {
            // Reprint banner without calling cls again
            auto ips = getLocalIPs();
            printBannerArt(g_port.load(), ips);
        }

    } else if (lo == "/exit" || lo == "/quit") {
        Log(L_WARN, "Shutting down server...");
        g_running = false;

    } else {
        Log(L_WARN, "Unknown command: " + cmd + "  (type /help)");
    }
}

static void printBanner(int port, const std::vector<std::string>& ips) {
    system("cls");
    printBannerArt(port, ips);
}

// ────────────────────────────────────────────────────────────────
//  PRINT CLI HELP
// ────────────────────────────────────────────────────────────────
static void printCliHelp(const char* exe) {
    std::cout << "\nlocalTransfer.io — Local network file transfer\n";
    std::cout << "Architected by: Nicanor III W. Cariasa (2026)\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << exe << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -p, --port <port>           Override port (default: auto-detect)\n";
    std::cout << "  -Sd, --saving_dir <path>    Set saving directory (default: Desktop)\n";
    std::cout << "  -h, --help                  Show this help and exit\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << exe << " --port 8080 --saving_dir C:\\Transfers\n";
    std::cout << "  " << exe << " -p 9090 -Sd D:\\Shared\n\n";
}

// ────────────────────────────────────────────────────────────────
//  MAIN
// ────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    g_hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleA("localTransfer.io");

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_hCon, &csbi)) {
        COORD bufSize = {140, 3000};
        SetConsoleScreenBufferSize(g_hCon, bufSize);
        SMALL_RECT rect = {0, 0, 139, 40};
        SetConsoleWindowInfo(g_hCon, TRUE, &rect);
    }

    // ── Parse CLI args ──
    int    cliPort    = -1;
    std::string cliSavingDir;
    bool   showHelp   = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { showHelp = true; break; }
        else if ((a == "-p" || a == "--port") && i+1 < argc) {
            try { cliPort = std::stoi(argv[++i]); } catch (...) { std::cerr << "Invalid port\n"; return 1; }
        } else if ((a == "-Sd" || a == "--saving_dir") && i+1 < argc) {
            cliSavingDir = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            printCliHelp(argv[0]); return 1;
        }
    }

    if (showHelp) { printCliHelp(argv[0]); return 0; }

    // ── Init Winsock ──
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[ERR] WSAStartup failed\n"; return 1;
    }

    // ── Determine saving directory ──
    // Priority: CLI arg > config file > Desktop
    if (!cliSavingDir.empty()) {
        g_savingDir = cliSavingDir;
        saveConfigSavingDir(cliSavingDir);
    } else {
        std::string fromConfig = loadConfigSavingDir();
        if (!fromConfig.empty()) {
            g_savingDir = fromConfig;
            Log(L_INFO, "Saving dir from config: " + fromConfig);
        } else {
            g_savingDir = getDesktopPath();
        }
    }

    // ── Load and update database ──
    Log(L_INFO, "Loading database...");
    dbLoad();
    dbUpdateCheck();
    { std::lock_guard<std::mutex> lk(g_dbMtx);
      Log(L_INFO, "Database: " + std::to_string(g_database.size()) + " entries"); }
    dbSave(); // save after cleanup

    // ── Load storage limit from config ──
    {
        uint64_t savedLimit = loadConfigStorageLimit();
        if (savedLimit > 0) {
            g_storageLimitBytes.store(savedLimit);
            Log(L_INFO, "Storage limit from config: " + formatBytes(savedLimit));
        }
    }

    // ── Load pastebin ──
    {
        std::string pb = loadConfigPastebin();
        std::lock_guard<std::mutex> lk(g_pastebinMtx);
        g_pastebin = pb;
    }

    // ── Find port ──
    int port = (cliPort > 0) ? cliPort : findOpenPort();
    if (port < 0) {
        Log(L_ERR, "No available port found! Exiting.");
        WSACleanup(); return 1;
    }
    g_port = port;

    auto ips = getLocalIPs();
    printBanner(port, ips);

    // ── Start server thread ──
    std::thread srv(serverThread, port);

    // ── Start heartbeat thread ──
    std::thread hb(heartbeatThread);

    Sleep(200);

    if (!g_running) {
        Log(L_ERR, "Server failed to start.");
        srv.join(); WSACleanup(); return 1;
    }

    Log(L_OK, "Server started on port " + std::to_string(port));
    Log(L_INFO, "Saving files to: " + getSavingDir());
    Log(L_INFO, "Type /help for commands. Type /exit to quit.\n");

    // ── Command loop ──
    while (g_running) {
        {
            std::lock_guard<std::mutex> lk(g_logMtx);
            SetConsoleTextAttribute(g_hCon, 11);
            std::cout << "  localTransfer.io> ";
            SetConsoleTextAttribute(g_hCon, 7);
            std::cout.flush();
        }
        std::string line = readLine();
        if (!line.empty()) processCommand(line);
    }

    // ── Shutdown ──
    Log(L_INFO, "Waiting for server thread...");
    g_running = false;
    // Close all SSE sockets so threads unblock
    { std::lock_guard<std::mutex> lk(g_sseMtx);
      for (SOCKET s : g_sseClients) closesocket(s);
      g_sseClients.clear(); }
    srv.join();
    hb.join();
    WSACleanup();
    Log(L_OK, "localTransfer.io stopped. Goodbye.");
    SetConsoleTextAttribute(g_hCon, 7);
    return 0;
}