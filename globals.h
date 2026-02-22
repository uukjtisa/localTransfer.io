// ================================================================
//  localTransfer.io  –  globals.h
//  Shared includes, structs, enums, and extern global declarations.
//  Every translation unit includes this file.
// ================================================================
#pragma once

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

// ────────────────────────────────────────────────────────────────
//  STRUCTS
// ────────────────────────────────────────────────────────────────
struct FileRecord {
    std::string name, savedPath, timestamp, from;
    uint64_t    size = 0;
};

struct DbEntry {
    std::string id, name, savedPath, timestamp, from;
    uint64_t    size{0};
};

struct ForwardingFolder {
    std::string              id;
    std::string              path;
    std::vector<std::string> contents;
};

// ────────────────────────────────────────────────────────────────
//  LOG LEVEL ENUM
// ────────────────────────────────────────────────────────────────
enum LogLvl { L_INFO=0, L_OK, L_WARN, L_ERR, L_VERB, L_CMD, L_NET };

// ────────────────────────────────────────────────────────────────
//  EXTERN GLOBALS  (defined in globals.cpp)
// ────────────────────────────────────────────────────────────────
extern HANDLE g_hCon;

extern std::atomic<bool>     g_verbose;
extern std::atomic<bool>     g_running;
extern std::atomic<int>      g_activeClients;
extern std::atomic<int>      g_port;
extern std::atomic<uint64_t> g_totalBytes;
extern std::atomic<uint64_t> g_fileCount;

extern std::mutex g_logMtx;
extern std::mutex g_filesMtx;

// Saving directory
extern std::string g_savingDir;
extern std::mutex  g_savingDirMtx;

// Input-redraw state
extern std::atomic<bool> g_inputActive;
extern std::string       g_inputBuf;     // always accessed under g_logMtx

// In-memory file list
extern std::vector<FileRecord> g_files;

// Database
extern std::vector<DbEntry> g_database;
extern std::mutex            g_dbMtx;

// SSE clients
extern std::vector<SOCKET> g_sseClients;
extern std::mutex          g_sseMtx;

// Pastebin
extern std::string g_pastebin;
extern std::mutex  g_pastebinMtx;

// Per-IP connection counting
extern std::map<std::string, int> g_ipCount;
extern std::mutex                 g_ipMtx;

// Per-level muting bitmask
extern std::atomic<uint32_t> g_mutedLevels;

// Storage limits
extern std::atomic<uint64_t> g_storageLimitBytes;
extern std::atomic<uint64_t> g_uploadLimitPct;

// Forwarding folders
extern std::vector<ForwardingFolder> g_ffFolders;
extern std::mutex                    g_ffMtx;
