// ================================================================
//  localTransfer.io  –  globals.cpp
//  Definitions of all global variables declared in globals.h.
// ================================================================
#include "globals.h"

HANDLE g_hCon = nullptr;

std::atomic<bool>     g_verbose{false};
std::atomic<bool>     g_running{true};
std::atomic<int>      g_activeClients{0};
std::atomic<int>      g_port{0};
std::atomic<uint64_t> g_totalBytes{0};
std::atomic<uint64_t> g_fileCount{0};

std::mutex g_logMtx;
std::mutex g_filesMtx;

std::string g_savingDir;
std::mutex  g_savingDirMtx;

std::atomic<bool> g_inputActive{false};
std::string       g_inputBuf;
std::atomic<int>  g_inputCursorPos{0};
std::atomic<int>  g_acShownLines{0};
std::atomic<int>  g_promptRow{0};

std::vector<FileRecord> g_files;

std::vector<DbEntry> g_database;
std::mutex           g_dbMtx;

std::vector<SOCKET> g_sseClients;
std::mutex          g_sseMtx;

std::string g_pastebin;
std::mutex  g_pastebinMtx;

std::map<std::string, int> g_ipCount;
std::mutex                 g_ipMtx;

std::atomic<uint32_t> g_mutedLevels{0};

std::atomic<uint64_t> g_storageLimitBytes{0};
std::atomic<uint64_t> g_uploadLimitPct{80};

std::vector<ForwardingFolder> g_ffFolders;
std::mutex                    g_ffMtx;
