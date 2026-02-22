// ================================================================
//  localTransfer.io  –  database.cpp
//  Database load/save/update, SSE broadcast, storage helpers,
//  and the heartbeat thread.
// ================================================================
#include "database.h"
#include "utils.h"

// ────────────────────────────────────────────────────────────────
//  STORAGE HELPERS
// ────────────────────────────────────────────────────────────────
uint64_t computeDbUsedBytes() {
    std::lock_guard<std::mutex> lk(g_dbMtx);
    uint64_t total = 0;
    for (auto& e : g_database) total += e.size;
    return total;
}

uint64_t getEffectiveUploadLimitBytes() {
    uint64_t cap = g_storageLimitBytes.load();
    uint64_t pct = g_uploadLimitPct.load();
    uint64_t available = (cap > 0) ? cap : getDiskFreeBytes(getDbPath());
    return (uint64_t)(available * pct / 100ULL);
}

// ────────────────────────────────────────────────────────────────
//  DATABASE
// ────────────────────────────────────────────────────────────────
std::string generateId() {
    SYSTEMTIME s; GetLocalTime(&s);
    char buf[32];
    sprintf_s(buf, "%04d%02d%02d%02d%02d%02d",
        s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond);
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1000, 9999);
    return std::string(buf) + "_" + std::to_string(dist(rng));
}

void dbSave() {
    std::lock_guard<std::mutex> lkDb(g_dbMtx);
    std::lock_guard<std::mutex> lkFf(g_ffMtx);
    std::ofstream f(getDbPath(), std::ios::trunc);
    if (!f.is_open()) { Log(L_WARN, "Cannot write database.json"); return; }

    f << "{\n\"files\":[\n";
    for (size_t i = 0; i < g_database.size(); ++i) {
        auto& e = g_database[i];
        f << "  {";
        f << "\"id\":\""        << jsonEscape(e.id)        << "\",";
        f << "\"name\":\""      << jsonEscape(e.name)      << "\",";
        f << "\"savedPath\":\"" << jsonEscape(e.savedPath) << "\",";
        f << "\"size\":"        << e.size                  << ",";
        f << "\"timestamp\":\"" << jsonEscape(e.timestamp) << "\",";
        f << "\"from\":\""      << jsonEscape(e.from)      << "\"";
        f << "}";
        if (i + 1 < g_database.size()) f << ",";
        f << "\n";
    }

    f << "],\n\"forwarding_folders\":[\n";
    for (size_t i = 0; i < g_ffFolders.size(); ++i) {
        auto& ff = g_ffFolders[i];
        f << "  {\"id\":\""   << jsonEscape(ff.id)   << "\",";
        f << "\"path\":\""    << jsonEscape(ff.path) << "\",";
        f << "\"subfolders_enabled\":" << (ff.subfoldersEnabled ? "true" : "false") << ",";
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

void dbLoad() {
    std::ifstream f(getDbPath());
    if (!f.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    std::string filesArr, ffArr;
    std::string trimmed = content;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && trimmed[first] == '{') {
        filesArr = jsGetArrayStr(content, "files");
        ffArr    = jsGetArrayStr(content, "forwarding_folders");
    } else {
        filesArr = content; // legacy plain-array format
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
            // Parse subfoldersEnabled
            {
                size_t sp = obj.find("\"subfolders_enabled\":");
                if (sp != std::string::npos) {
                    std::string rest = obj.substr(sp + 21);
                    rest = rest.substr(rest.find_first_not_of(" \t\r\n"));
                    ff.subfoldersEnabled = (rest.rfind("true",0)==0);
                }
            }
            std::string contArr = jsGetArrayStr(obj, "contents");
            if (!contArr.empty()) {
                size_t p = 1;
                while (p < contArr.size()) {
                    while (p < contArr.size() && contArr[p] != '"' && contArr[p] != ']') ++p;
                    if (p >= contArr.size() || contArr[p] == ']') break;
                    ++p;
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

void dbUpdateCheck() {
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

void dbAddEntry(const DbEntry& e) {
    { std::lock_guard<std::mutex> lk(g_dbMtx); g_database.push_back(e); }
    dbSave();
}

// ────────────────────────────────────────────────────────────────
//  RECURSIVE TREE HELPERS (used by heartbeat + buildDbJson)
// ────────────────────────────────────────────────────────────────
static FfTreeEntry buildTree(const std::string& dirPath) {
    FfTreeEntry node;
    node.fullPath = dirPath;
    node.isDir    = true;
    // get basename
    size_t sl = dirPath.rfind('\\');
    if (sl == std::string::npos) sl = dirPath.rfind('/');
    node.name = (sl != std::string::npos) ? dirPath.substr(sl+1) : dirPath;

    std::wstring wpattern = toWide(dirPath + "\\*");
    WIN32_FIND_DATAW fdW;
    HANDLE h = FindFirstFileW(wpattern.c_str(), &fdW);
    if (h == INVALID_HANDLE_VALUE) return node;
    do {
        std::wstring wn = fdW.cFileName;
        if (wn == L"." || wn == L"..") continue;
        char fnUtf8[MAX_PATH * 4] = {};
        WideCharToMultiByte(CP_UTF8, 0, fdW.cFileName, -1, fnUtf8, sizeof(fnUtf8), nullptr, nullptr);
        std::string childPath = dirPath + "\\" + fnUtf8;

        if (fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            node.children.push_back(buildTree(childPath));
        } else {
            FfTreeEntry file;
            file.fullPath = childPath;
            file.name     = fnUtf8;
            file.isDir    = false;
            LARGE_INTEGER li;
            li.HighPart = fdW.nFileSizeHigh;
            li.LowPart  = fdW.nFileSizeLow;
            file.size   = (uint64_t)li.QuadPart;
            node.children.push_back(file);
        }
    } while (FindNextFileW(h, &fdW));
    FindClose(h);
    return node;
}

static std::string treeToJson(const FfTreeEntry& node) {
    std::string j = "{";
    j += "\"name\":\"" + jsonEscape(node.name) + "\",";
    j += "\"path\":\"" + jsonEscape(node.fullPath) + "\",";
    j += "\"isDir\":"  + std::string(node.isDir ? "true" : "false") + ",";
    j += "\"size\":"   + std::to_string(node.size);
    if (node.isDir) {
        j += ",\"children\":[";
        for (size_t i = 0; i < node.children.size(); ++i) {
            if (i) j += ",";
            j += treeToJson(node.children[i]);
        }
        j += "]";
    }
    j += "}";
    return j;
}

// ────────────────────────────────────────────────────────────────
//  SSE BROADCAST
// ────────────────────────────────────────────────────────────────
std::string buildDbJson() {
    std::lock_guard<std::mutex> lk(g_dbMtx);
    std::lock_guard<std::mutex> lkFf(g_ffMtx);
    std::string json = "{\"files\":[";
    for (size_t i = 0; i < g_database.size(); ++i) {
        auto& e = g_database[i];
        json += "{\"id\":\""        + jsonEscape(e.id)        + "\",";
        json += "\"name\":\""       + jsonEscape(e.name)      + "\",";
        json += "\"size\":"         + std::to_string(e.size)  + ",";
        json += "\"timestamp\":\"" + jsonEscape(e.timestamp)  + "\",";
        json += "\"from\":\""       + jsonEscape(e.from)      + "\"}";
        if (i + 1 < g_database.size()) json += ",";
    }
    json += "],\"forwarding_folders\":[";
    for (size_t i = 0; i < g_ffFolders.size(); ++i) {
        auto& ff = g_ffFolders[i];
        json += "{\"id\":\""   + jsonEscape(ff.id)   + "\",";
        json += "\"path\":\"" + jsonEscape(ff.path) + "\",";
        json += "\"subfolders_enabled\":" + std::string(ff.subfoldersEnabled ? "true" : "false") + ",";
        if (ff.subfoldersEnabled) {
            // Send full recursive tree
            json += "\"tree\":" + treeToJson(ff.tree) + ",";
        }
        json += "\"contents\":[";
        for (size_t j = 0; j < ff.contents.size(); ++j) {
            std::string fp = ff.contents[j];
            size_t sl = fp.rfind('\\');
            if (sl == std::string::npos) sl = fp.rfind('/');
            std::string fname = (sl != std::string::npos) ? fp.substr(sl+1) : fp;
            json += "{\"name\":\"" + jsonEscape(fname) + "\",";
            json += "\"path\":\""  + jsonEscape(fp)    + "\"}";
            if (j + 1 < ff.contents.size()) json += ",";
        }
        json += "]}";
        if (i + 1 < g_ffFolders.size()) json += ",";
    }
    json += "]}";
    return json;
}

void sseBroadcast(const std::string& event, const std::string& data) {
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

void sseBroadcastDb() {
    sseBroadcast("db_update", buildDbJson());
}

void ssePing() {
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

void sseBroadcastPastebin() {
    std::string content;
    { std::lock_guard<std::mutex> lk(g_pastebinMtx); content = g_pastebin; }
    sseBroadcast("pastebin_update", "{\"content\":\"" + jsonEscape(content) + "\"}");
}

// ────────────────────────────────────────────────────────────────
//  HEARTBEAT THREAD  (500 ms: prune stale DB paths, scan FF, SSE keepalive)
// ────────────────────────────────────────────────────────────────
void heartbeatThread() {
    int pingTick = 0;
    while (g_running) {
        Sleep(500);
        ++pingTick;
        bool changed = false;

        // Prune stale database entries
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

        // Scan forwarding folders
        {
            std::lock_guard<std::mutex> lk(g_ffMtx);
            for (auto& ff : g_ffFolders) {
                if (ff.subfoldersEnabled) {
                    // Rebuild full tree recursively
                    FfTreeEntry newTree = buildTree(ff.path);
                    // Compare by serializing to detect changes (simple but works)
                    std::string oldJson = treeToJson(ff.tree);
                    std::string newJson = treeToJson(newTree);
                    if (oldJson != newJson) {
                        ff.tree = std::move(newTree);
                        changed = true;
                    }
                    // Also keep flat contents for backward compat
                    std::vector<std::string> found;
                    std::wstring wpattern = toWide(ff.path + "\\*");
                    WIN32_FIND_DATAW fdW;
                    HANDLE h = FindFirstFileW(wpattern.c_str(), &fdW);
                    if (h != INVALID_HANDLE_VALUE) {
                        do {
                            if (fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                            char fnUtf8[MAX_PATH * 4] = {};
                            WideCharToMultiByte(CP_UTF8, 0, fdW.cFileName, -1, fnUtf8, sizeof(fnUtf8), nullptr, nullptr);
                            found.push_back(ff.path + "\\" + fnUtf8);
                        } while (FindNextFileW(h, &fdW));
                        FindClose(h);
                    }
                    if (found != ff.contents) { ff.contents = found; changed = true; }
                } else {
                    std::vector<std::string> found;
                    std::wstring wpattern = toWide(ff.path + "\\*");
                    WIN32_FIND_DATAW fdW;
                    HANDLE h = FindFirstFileW(wpattern.c_str(), &fdW);
                    if (h != INVALID_HANDLE_VALUE) {
                        do {
                            if (fdW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
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
        }

        if (changed) {
            dbSave();
            sseBroadcastDb();
        }

        // SSE keepalive ping every 20 seconds
        if (pingTick % 40 == 0) ssePing();
    }
}
