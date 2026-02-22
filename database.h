// ================================================================
//  localTransfer.io  –  database.h
//  Database, SSE broadcast, storage helpers, and heartbeat thread.
// ================================================================
#pragma once
#include "globals.h"

// ── Storage helpers ──
uint64_t computeDbUsedBytes();
uint64_t getEffectiveUploadLimitBytes();

// ── Database ──
std::string generateId();
void        dbSave();
void        dbLoad();
void        dbUpdateCheck();
void        dbAddEntry(const DbEntry& e);

// ── SSE broadcast ──
std::string buildDbJson();
void        sseBroadcast(const std::string& event, const std::string& data);
void        sseBroadcastDb();
void        ssePing();
void        sseBroadcastPastebin();

// ── Heartbeat thread ──
void heartbeatThread();
