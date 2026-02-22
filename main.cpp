// ================================================================
//  localTransfer.io  –  main.cpp
//  Banner, command processor, input loop, and program entry point.
//
//  Architected by: Nicanor III W. Cariasa (2026)
//  Built by: Claude Sonnet 4.6
// ================================================================
#include "globals.h"
#include "utils.h"
#include "database.h"
#include "http_server.h"

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