// ================================================================
//  localTransfer.io  –  mdns.h
//  Minimal multicast-DNS responder so the server can be reached as
//  http://localTransfer.io.local/ with no configuration on any device.
//
//  Scope, deliberately: this answers A queries for one hostname and
//  advertises one _http._tcp service. It is not a general mDNS stack
//  and does not attempt conflict resolution beyond a name probe.
//
//  Reality check: Chrome on Android does NOT resolve .local names
//  typed into the address bar. iOS, macOS and Windows do. The numeric
//  http://<ip>:<port>/ URLs remain the path that always works.
// ================================================================
#pragma once
#include "globals.h"

// Every .local name we answer to. Measured behaviour, not theory:
//
//   localtransfer-io.local   single label — resolves on Windows. USE THIS.
//   localtransfer.local      single label — resolves on Windows.
//   localTransfer.io.local   multi-label  — answered on the wire, but the
//                            Windows resolver never even sends the query for
//                            it. Kept because Apple's Bonjour is permissive.
//
// Answering several names costs a few bytes per announcement and nothing else.
extern std::vector<std::string> g_mdnsNames;

// Start/stop the responder thread. Safe to call stop when never started.
void mdnsStart(int servicePort);
void mdnsStop();

// The name to show people: the first one that actually resolves everywhere.
std::string mdnsFullName();
