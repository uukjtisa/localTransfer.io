// ================================================================
//  localTransfer.io  –  utils.h
//  Utility function declarations:
//    logger, string helpers, wide-string, disk/config/JSON helpers,
//    path helpers, IP helpers.
// ================================================================
#pragma once
#include "globals.h"

// ── Logger ──
void Log(LogLvl lvl, const std::string& msg);

// ── String helpers ──
std::string trim(const std::string& s);
std::string formatBytes(uint64_t b);
std::string nowStr();
std::string nowHuman();

// ── Wide-string helper ──
std::wstring toWide(const std::string& utf8);

// ── Path / exe helpers ──
std::string getExeDir();
std::string getDbPath();
std::string getConfigPath();
std::string getDesktopPath();

// ── Saving-dir accessors ──
std::string getSavingDir();
void        setSavingDir(const std::string& dir);

// ── Network ──
std::vector<std::string> getLocalIPs();

// ── Disk space ──
std::string getDiskRoot(const std::string& path);
uint64_t    getDiskFreeBytes(const std::string& path);
uint64_t    getDiskTotalBytes(const std::string& path);
uint64_t    parseHumanSize(const std::string& s);

// ── Config file (.localTransfer.config) ──
std::string loadConfigSavingDir();
uint64_t    loadConfigStorageLimit();
std::string loadConfigPastebin();
void        saveConfigSavingDir(const std::string& dir);
void        saveConfigStorageLimit(uint64_t bytes);
void        saveConfigPastebin(const std::string& content);

// ── JSON helpers ──
std::string              jsonEscape(const std::string& s);
std::string              jsonUnescape(const std::string& s);
std::string              jsGetStr(const std::string& obj, const std::string& key);
uint64_t                 jsGetU64(const std::string& obj, const std::string& key);
std::string              jsGetArrayStr(const std::string& json, const std::string& key);
std::vector<std::string> jsParseArray(const std::string& json);


