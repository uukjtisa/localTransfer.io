// ================================================================
//  localTransfer.io  –  utils.cpp
//  Utility function definitions.
// ================================================================
#include "utils.h"

// ────────────────────────────────────────────────────────────────
//  LOGGER
// ────────────────────────────────────────────────────────────────
static const WORD LVL_COLORS[] = { 15, 10, 14, 12, 9, 13, 11 };
static const char* LVL_TAGS[]  = {
    "[INFO]  ", "[OK]    ", "[WARN]  ", "[ERR]   ",
    "[VERB]  ", "[CMD]   ", "[NET]   "
};

std::string nowStr() {
    SYSTEMTIME s; GetLocalTime(&s);
    char b[16];
    sprintf_s(b, "%02d:%02d:%02d", s.wHour, s.wMinute, s.wSecond);
    return b;
}

void Log(LogLvl lvl, const std::string& msg) {
    if (lvl == L_VERB && !g_verbose) return;
    if (g_mutedLevels.load() & (1u << (unsigned)lvl)) return;

    std::lock_guard<std::mutex> lk(g_logMtx);

    bool wasInput = g_inputActive.load();
    if (wasInput) {
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
//  STRING HELPERS
// ────────────────────────────────────────────────────────────────
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

std::string formatBytes(uint64_t b) {
    char buf[64];
    if (b < 1024)            sprintf_s(buf, "%llu B",  b);
    else if (b < 1048576)    sprintf_s(buf, "%.1f KB", b/1024.0);
    else if (b < 1073741824) sprintf_s(buf, "%.2f MB", b/1048576.0);
    else                     sprintf_s(buf, "%.2f GB", b/1073741824.0);
    return buf;
}

std::string nowHuman() {
    SYSTEMTIME s; GetLocalTime(&s);
    char b[32];
    sprintf_s(b, "%04d-%02d-%02d %02d:%02d:%02d",
        s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond);
    return b;
}

// ────────────────────────────────────────────────────────────────
//  WIDE-STRING HELPER
// ────────────────────────────────────────────────────────────────
std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &w[0], n);
    return w;
}

// ────────────────────────────────────────────────────────────────
//  PATH / EXE HELPERS
// ────────────────────────────────────────────────────────────────
std::string getExeDir() {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p(path);
    size_t pos = p.rfind('\\');
    return (pos != std::string::npos) ? p.substr(0, pos) : ".";
}

std::string getDbPath()     { return getExeDir() + "\\database.json"; }
std::string getConfigPath() { return getExeDir() + "\\.localTransfer.config"; }

std::string getDesktopPath() {
    WCHAR path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, path))) {
        char narrow[MAX_PATH] = {};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, MAX_PATH, nullptr, nullptr);
        return std::string(narrow);
    }
    return ".";
}

// ────────────────────────────────────────────────────────────────
//  SAVING-DIR ACCESSORS
// ────────────────────────────────────────────────────────────────
std::string getSavingDir() {
    std::lock_guard<std::mutex> lk(g_savingDirMtx);
    return g_savingDir;
}

void setSavingDir(const std::string& dir) {
    { std::lock_guard<std::mutex> lk(g_savingDirMtx); g_savingDir = dir; }
    saveConfigSavingDir(dir);
}

// ────────────────────────────────────────────────────────────────
//  NETWORK
// ────────────────────────────────────────────────────────────────
std::vector<std::string> getLocalIPs() {
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

// ────────────────────────────────────────────────────────────────
//  DISK SPACE
// ────────────────────────────────────────────────────────────────
std::string getDiskRoot(const std::string& path) {
    if (path.size() >= 3 && path[1] == ':')
        return path.substr(0, 3);
    char full[MAX_PATH] = {};
    GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr);
    std::string fp(full);
    if (fp.size() >= 3 && fp[1] == ':') return fp.substr(0, 3);
    return "C:\\";
}

uint64_t getDiskFreeBytes(const std::string& path) {
    std::string root = getDiskRoot(path);
    ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFree{};
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvail, &totalBytes, &totalFree))
        return (uint64_t)freeBytesAvail.QuadPart;
    return 0;
}

uint64_t getDiskTotalBytes(const std::string& path) {
    std::string root = getDiskRoot(path);
    ULARGE_INTEGER freeBytesAvail{}, totalBytes{}, totalFree{};
    if (GetDiskFreeSpaceExA(root.c_str(), &freeBytesAvail, &totalBytes, &totalFree))
        return (uint64_t)totalBytes.QuadPart;
    return 0;
}

uint64_t parseHumanSize(const std::string& s) {
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
std::string loadConfigSavingDir() {
    std::ifstream f(getConfigPath());
    if (!f.is_open()) return "";
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("saving_dir=", 0) == 0) return line.substr(11);
    return "";
}

uint64_t loadConfigStorageLimit() {
    std::ifstream f(getConfigPath());
    if (!f.is_open()) return 0;
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("storage_limit=", 0) == 0)
            try { return std::stoull(line.substr(14)); } catch (...) {}
    return 0;
}

std::string loadConfigPastebin() {
    std::ifstream f(getConfigPath());
    if (!f.is_open()) return "";
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("pastebin=", 0) == 0) {
            std::string enc = line.substr(9);
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

void saveConfigSavingDir(const std::string& dir) {
    std::vector<std::string> lines;
    {
        std::ifstream f(getConfigPath());
        std::string l;
        while (std::getline(f, l))
            if (l.rfind("saving_dir=", 0) != 0) lines.push_back(l);
    }
    std::ofstream f(getConfigPath(), std::ios::trunc);
    f << "saving_dir=" << dir << "\n";
    for (auto& l : lines) f << l << "\n";
}

void saveConfigStorageLimit(uint64_t bytes) {
    std::vector<std::string> lines;
    {
        std::ifstream f(getConfigPath());
        std::string l;
        while (std::getline(f, l))
            if (l.rfind("storage_limit=", 0) != 0) lines.push_back(l);
    }
    std::ofstream f(getConfigPath(), std::ios::trunc);
    if (bytes > 0) f << "storage_limit=" << bytes << "\n";
    for (auto& l : lines) f << l << "\n";
}

void saveConfigPastebin(const std::string& content) {
    std::vector<std::string> lines;
    {
        std::ifstream f(getConfigPath());
        std::string l;
        while (std::getline(f, l))
            if (l.rfind("pastebin=", 0) != 0 && l.rfind("saving_dir=", 0) != 0)
                lines.push_back(l);
    }
    std::string enc;
    for (char c : content) {
        if (c == '\n') enc += "\\n";
        else if (c == '\r') {}
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
//  JSON HELPERS
// ────────────────────────────────────────────────────────────────
std::string jsonEscape(const std::string& s) {
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

std::string jsonUnescape(const std::string& s) {
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

std::string jsGetStr(const std::string& obj, const std::string& key) {
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

uint64_t jsGetU64(const std::string& obj, const std::string& key) {
    std::string k = "\"" + key + "\":";
    size_t p = obj.find(k);
    if (p == std::string::npos) return 0;
    p += k.size();
    while (p < obj.size() && (obj[p]==' '||obj[p]=='\t')) ++p;
    if (p >= obj.size()) return 0;
    try { return (uint64_t)std::stoull(obj.substr(p)); } catch (...) { return 0; }
}

std::string jsGetArrayStr(const std::string& json, const std::string& key) {
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

std::vector<std::string> jsParseArray(const std::string& json) {
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
