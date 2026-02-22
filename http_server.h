// ================================================================
//  localTransfer.io  –  http_server.h
//  HTTP server declarations: port detection, server thread.
//  The HTML page and all internal helpers live in http_server.cpp.
// ================================================================
#pragma once
#include "globals.h"

// ── Port detection ──
bool portAvailable(int port);
int  findOpenPort();

// ── Server thread (launched from main) ──
void serverThread(int port);
