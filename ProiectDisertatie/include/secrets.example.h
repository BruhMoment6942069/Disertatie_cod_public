#ifndef SECRETS_H
#define SECRETS_H

// ═══════════════════════════════════════════════════════════════════════════════
// TEMPLATE — copy this file to secrets.h and fill in your own values
// ═══════════════════════════════════════════════════════════════════════════════
//
//   cp secrets.example.h secrets.h
//
// secrets.h is listed in .gitignore and never reaches the repository. This file
// is tracked, so it must never contain a real password.
//
// Two networks are tried in order. The first is the normal one; the second is a
// fallback, useful for demonstrating the rig somewhere the first does not exist.
// Both entries must be present even if you only use one — point the second at
// the same network if you have no fallback.
//
// remoteIp is where the ESP32 would stream telemetry if no handshake had been
// received. In practice the destination always comes from the PC's discovery
// broadcast, so this value is currently unused; keep it valid anyway.

const char* ssid      = "YOUR_WIFI_SSID";
const char* password  = "YOUR_WIFI_PASSWORD";
const char* remoteIp  = "192.168.1.255";

const char* ssid2     = "YOUR_FALLBACK_SSID";
const char* password2 = "YOUR_FALLBACK_PASSWORD";
const char* remoteIp2 = "192.168.1.255";

#endif // SECRETS_H
