#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
// Serial monitor colors
// ═══════════════════════════════════════════════════════════════════════════════
#define C_RED   "\033[31m"
#define C_GRN   "\033[32m"
#define C_YEL   "\033[33m"
#define C_CYN   "\033[36m"
#define C_RST   "\033[0m"   // reset — always end with this
// ═══════════════════════════════════════════════════════════════════════════════
// NETWORK
// ═══════════════════════════════════════════════════════════════════════════════
bool connectionEstablished = false;
char        udpBuffer[256];

// Real SSIDs and passwords live in secrets.h, which is NOT tracked by git.
// Copy secrets.example.h to secrets.h and fill in your own values before the
// first build. It defines: ssid, password, remoteIp, ssid2, password2, remoteIp2.
#include "secrets.h"

constexpr int localPort  = 1234;
constexpr int localPort2 = 1234;   // fallback network, same port

// ─── Timing ──────────────────────────────────────────────────────────────────
constexpr unsigned long WIFI_TIMEOUT_MS    = 5000UL;  // ms before trying fallback
constexpr unsigned long UDP_STREAM_RATE_MS = 20UL;     // ms between position samples (50Hz)

// ─── General ──────────────────────────────────────────────────────────────────
#define ENDSTOP_CLEARANCE   160
#define M1_KP               3.0f
#define M2_KP               3.0f
#define M3_KP               3.0f
#define M4_KP               3.0f
#define CONTROL_PERIOD_US   1000
#define MIN_MOVE_DELTA      3.0f
constexpr uint8_t MSG_MOTOR_POS = 1;
#define STEPS_PER_REV       3200.0f
constexpr float_t stepsPerDegree = STEPS_PER_REV / 360.0;
#define DEADBAND            48
constexpr unsigned long SPEED_TIMEOUT_MS = 300UL;




// ─── Modes ────────────────────────────────────────────────────────────────────
constexpr uint8_t MSG_TARGET = 2;
constexpr uint8_t MSG_MODE   = 3;
constexpr uint8_t MODE_TRACK = 0;
constexpr uint8_t MODE_TEST  = 1;
constexpr uint8_t MSG_SPEED  = 4;


// ═══════════════════════════════════════════════════════════════════════════════
// MOTOR 1 — PAN
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Pins ────────────────────────────────────────────────────────────────────
#define M1_STEP_PIN         26
#define M1_DIR_PIN          33
// NOTE: 15 is a strapping pin - it must read HIGH at reset. INPUT_PULLUP holds it there,
// but if this endstop is PRESSED at power-on it pulls 15 low and the board may not boot.
// If that ever happens, move this endstop to a non-strapping pin (16, 17 are free).
#define M1_ENDSTOP_PIN      15


// ─── Mechanics ───────────────────────────────────────────────────────────────
#define M1_RANGE_DEG        200.0f              // physical range in degrees
#define M1_RANGE_STEPS      (static_cast<int32_t>(STEPS_PER_REV * M1_RANGE_DEG / 360.0f)) // Formula: STEPS_PER_REV * (RANGE_DEG / 360) -> 270deg -> 3200 * 0.75 = 2400 steps

// Where this axis parks after homing, in degrees from home. Home (position 0) stays at
// the backoff point - this drives PAST it so the camera ends up looking forward, and
// deliberately does NOT re-zero. The parked step count is what the PC calls zeroSteps.
// TUNE BY EYE and reflash until the cameras look straight down the range.
#define M1_PARK_DEG         75.0f

// ─── Motion ──────────────────────────────────────────────────────────────────
#define M1_HOMING_SPEED     400.0f
#define M1_HOMING_ACCEL     250.0f
#define M1_MOVE_SPEED       1000.0f
#define M1_MOVE_ACCEL       500.0f
#define M1_MIN_SPEED        0.5
#define DIR_M1         (-1) //homing direction

// ═══════════════════════════════════════════════════════════════════════════════
// MOTOR 2 — TILT
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Pins ────────────────────────────────────────────────────────────────────
#define M2_STEP_PIN         25
#define M2_DIR_PIN          32
#define M2_ENDSTOP_PIN      4

// ─── Mechanics ───────────────────────────────────────────────────────────────
#define M2_RANGE_DEG        60.0f
#define M2_RANGE_STEPS      (static_cast<int32_t>((STEPS_PER_REV * M2_RANGE_DEG) / 360.0f))

#define M2_PARK_DEG         30.0f              // roughly level. TUNE BY EYE.

// ─── Motion ──────────────────────────────────────────────────────────────────
#define M2_HOMING_SPEED     400.0f
#define M2_HOMING_ACCEL     250.0f
#define M2_MOVE_SPEED       1000.0f
#define M2_MOVE_ACCEL       500.0f
#define M2_MIN_SPEED        0.5
#define DIR_M2         (-1)

// ═══════════════════════════════════════════════════════════════════════════════
// MOTOR 3 — PAN (gimbal 2)
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Pins ────────────────────────────────────────────────────────────────────
// DIR deliberately NOT on 34-39: those are input-only on ESP32, so digitalWrite is a
// silent no-op and the axis would only ever turn one way.
#define M3_STEP_PIN         27
#define M3_DIR_PIN          21
#define M3_ENDSTOP_PIN      19


// ─── Mechanics ───────────────────────────────────────────────────────────────
#define M3_RANGE_DEG        200.0f              // physical range in degrees
#define M3_RANGE_STEPS      (static_cast<int32_t>(STEPS_PER_REV * M3_RANGE_DEG / 360.0f)) // Formula: STEPS_PER_REV * (RANGE_DEG / 360) -> 270deg -> 3200 * 0.75 = 2400 steps

#define M3_PARK_DEG         75.0f              // identical head to M1. TUNE BY EYE.

// ─── Motion ──────────────────────────────────────────────────────────────────
#define M3_HOMING_SPEED     400.0f
#define M3_HOMING_ACCEL     250.0f
#define M3_MOVE_SPEED       1000.0f
#define M3_MOVE_ACCEL       500.0f
#define M3_MIN_SPEED        0.5
#define DIR_M3         (-1) //homing direction

// ═══════════════════════════════════════════════════════════════════════════════
// MOTOR 4 — TILT (gimbal 2)
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Pins ────────────────────────────────────────────────────────────────────
// 14 is free for STEP only because M2's DIR moved off it to 32.
#define M4_STEP_PIN         14
#define M4_DIR_PIN          22
#define M4_ENDSTOP_PIN      23

// ─── Mechanics ───────────────────────────────────────────────────────────────
#define M4_RANGE_DEG        60.0f
#define M4_RANGE_STEPS      (static_cast<int32_t>((STEPS_PER_REV * M4_RANGE_DEG) / 360.0f))

#define M4_PARK_DEG         30.0f              // identical head to M2. TUNE BY EYE.

// ─── Motion ──────────────────────────────────────────────────────────────────
#define M4_HOMING_SPEED     400.0f
#define M4_HOMING_ACCEL     250.0f
#define M4_MOVE_SPEED       1000.0f
#define M4_MOVE_ACCEL       500.0f
#define M4_MIN_SPEED        0.5
#define DIR_M4         (-1)

// ═══════════════════════════════════════════════════════════════════════════════
// SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════

const int           UDP_PORT            = 1234;     // port for all UDP traffic
const int           SERIAL_BAUD         = 115200;














#endif