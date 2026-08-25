---
title: SpeedStepper API Reference
aliases: [SpeedStepper, Stepper API, Motor API]
tags: [esp32, stepper, motor, firmware, reference, disertatie]
created: 2026-06-22
source: lib/SpeedStepper/SpeedStepper.h
---

# 🛠️ SpeedStepper — API Reference

> [!abstract] What is this?
> `SpeedStepper` is a **speed-based** stepper driver library (a fork of AccelStepper using David Austin's real-time profile equations). You command a **speed**, it ramps to it under an acceleration limit, and it respects soft position limits. This note documents every **public** method you can call.

---

## 🧠 The mental model (read this first)

> [!important] It controls *speed*, not *position*
> There is **no** `moveTo(position)`. The usage pattern is always:
> 1. Configure `setMaxSpeed()` + `setAcceleration()` **once**.
> 2. Command motion with `setSpeed(v)`.
> 3. Call `run()` **constantly** so it actually emits step pulses and ramps.
> 4. Use **limits** (`setPlusLimit` / `setMinusLimit`) as soft endstops.
>
> To move to a *position*, you close that loop yourself — read `getCurrentPosition()`, compute an error, and `setSpeed()` toward it. That's exactly what `trackAxis()` does in `main.cpp`.

> [!tip] One-liner to remember
> `setSpeed()` **says where to go**; `run()` **is the heartbeat that gets you there.** No `run()`, no movement.

---

## 📇 Quick index

| Group | Methods |
|---|---|
| [[#🔧 Construction]] | `SpeedStepper()` |
| [[#🚀 Core motion]] | `setSpeed` · `run` · `setMaxSpeed` · `setMinSpeed` · `setAcceleration` · `stop` · `hardStop` · `hardStart` |
| [[#📈 Reading speed]] | `getSetSpeed` · `getSpeed` |
| [[#📍 Position & limits]] | `getCurrentPosition` · `setCurrentPosition` · `setPlusLimit` · `getPlusLimit` · `setMinusLimit` · `getMinusLimit` |
| [[#🏠 Built-in homing]] | `goHome` · `isGoingHome` · `stopAndSetHome` |
| [[#🧭 Direction]] | `isDirForward` · `setDirForward` · `setDirReverse` · `invertDirectionLogic` |
| [[#🚦 Status]] | `isRunning` |
| [[#🎞️ Speed profiles]] | `setProfile` · `startProfile` · `stopProfile` · `isProfileRunning` |
| [[#🐞 Debug & constants]] | `setDebugPrint` · `MAX_INT32_T` |

---

## 🔧 Construction

### `SpeedStepper(int stepPin, int dirPin)`
Binds the STEP and DIR GPIOs and sets them as outputs. Create once — usually as a global.

```cpp
SpeedStepper m1Pan(M1_STEP_PIN, M1_DIR_PIN);   // STEP 26, DIR 12
```

> [!warning] Strapping pins
> Avoid ESP32 **strapping pins** for STEP/DIR (e.g. GPIO **0, 2, 12, 15**). Driving them can break boot or flash-voltage selection. `M1_DIR` on GPIO 12 is risky — prefer 14/16/17/27.

---

## 🚀 Core motion

> [!note] This is the 90% you actually use day-to-day.

### `void setSpeed(float sp)`
**Primary command.** Target speed in steps/sec — `+` forward, `−` reverse. The motor *ramps* to it at the acceleration rate.
- `|sp| < minSpeed` → treated as `0` (stop).
- Cancels `goHome()`.
- Does nothing visible until you call `run()`.

### `boolean run()`
**The heartbeat — call every loop iteration.** Emits a step when one is due, recomputes the next interval/speed, advances profiles. Returns `true` while still moving.

> [!danger] Don't starve `run()`
> `run()` must be called thousands of times/sec for smooth pulses. Blocking work in the loop (e.g. `Serial.println` every iteration) starves it and the motor stutters or stalls.

### `void setMaxSpeed(float maxSp)`
Caps `|speed|` (hard limit ≤ **1000** steps/sec). `setSpeed` clamps to this.

### `void setMinSpeed(float minSp)`
Floor (> 0.0003). Below it, a commanded speed is treated as `0` — the motor won't creep slower than this.

### `void setAcceleration(float a)`
Ramp rate in steps/sec². Low accel = long, gentle ramps **and** long stopping distance.

### `void stop()`
Sets speed `0` and **decelerates smoothly** to a halt (respects acceleration).

### `void hardStop()`
Stops **immediately**, no deceleration — zeroes speed and step interval.

> [!example] When to use which stop
> - **Endstop hit / emergency** → `hardStop()` (instant; `stop()` would coast into the switch).
> - **Normal "come to rest"** → `stop()` (smooth).

### `void hardStart(float sp)`
Jumps **instantly** to `sp`, ignoring the acceleration ramp (still respects speed/position limits). Aggressive — big current spike. Use sparingly.

---

## 📈 Reading speed

### `float getSetSpeed()`
The target speed you last requested via `setSpeed`.

### `float getSpeed()`
The **actual current** speed — differs from the set speed mid-ramp or while held at a limit.

---

## 📍 Position & limits

> [!info] Limits = soft endstops
> The motor decelerates and stops at a limit automatically. Combined with homing, this keeps an axis inside its physical travel.

### `int32_t getCurrentPosition()`
Current step count — your position feedback (read by `trackAxis`).

### `void setCurrentPosition(int32_t pos)`
Defines "where I am now" in steps (clamped into the limits). Used to set **zero** after homing.

### `void setPlusLimit(int32_t mPos)` · `int32_t getPlusLimit()`
Upper soft limit (steps, ≥ 0).

### `void setMinusLimit(int32_t mPos)` · `int32_t getMinusLimit()`
Lower soft limit (≤ 0).

> [!bug] Order matters — the limit-clamp trap
> `setPlusLimit` refuses to go **below** `currentPosition`, and `setMinusLimit` refuses to go **above** it — the limits always bracket where you are. So:
> 1. Open limits wide (`±MAX_INT32_T`) **before** moving freely (homing approach).
> 2. `setCurrentPosition(0)` to define zero.
> 3. *Then* install the tight `[0, range]` limits.
>
> Set a tight limit before zeroing and it gets silently clamped to your current position.

---

## 🏠 Built-in homing

> [!note] You wrote your own endstop homing (`motorPhase`) instead — these are the *library's* helpers.

### `void goHome()`
Drives toward logical position **0** at max speed and stops there. This is "return to the zero you defined," **not** endstop homing.

### `boolean isGoingHome()`
`true` while `goHome` is active. Any `setSpeed` call cancels it.

### `void stopAndSetHome()`
`hardStop()` then `setCurrentPosition(0)` — declare "here = 0."

---

## 🧭 Direction

### `boolean isDirForward()`
Current direction (`true` = forward).

### `void setDirForward()` / `void setDirReverse()`
Force direction and write the DIR pin. Low-level — normally direction follows the **sign** of `setSpeed`.

### `void invertDirectionLogic()`
**Flips DIR-pin polarity** (which level = forward). Toggle once to invert, again to revert.

> [!tip] The clean fix for a backwards motor
> If an axis spins/homes the wrong way, call `invertDirectionLogic()` once in setup for that motor — cleaner than rewiring or fighting `HOME_DIR`.

---

## 🚦 Status

### `boolean isRunning()`
`true` if a step is scheduled (`stepInterval != 0`) — it'll move at some future time.

---

## 🎞️ Speed profiles
*Advanced — scripted speed-vs-time motion. Probably overkill for pan/tilt tracking.*

```cpp
struct SpeedProfileStruct {
  float speed;            // target speed at the end of this segment
  unsigned long deltaTms; // ms to ramp from current speed to that target
};
```

| Method | Purpose |
|---|---|
| `void setProfile(SpeedProfileStruct* arr, size_t len)` | Define a sequence of `{speed, deltaTms}` segments. |
| `void startProfile()` | Run the profile from the current speed (driven by `run()`). |
| `void stopProfile()` | Abort the profile and stop. |
| `bool isProfileRunning()` | `true` while a profile is executing. |

---

## 🐞 Debug & constants

### `void setDebugPrint(Print* p)`
Routes internal debug to a stream: `m1.setDebugPrint(&Serial);`.

> [!warning] Only works if `#define DEBUG` is uncommented in `SpeedStepper.cpp`. Otherwise it's a no-op.

### `static const int32_t MAX_INT32_T = 0x3ffffff0`
The library's "infinity" for limits (kept under real `INT32_MAX` so `distanceToGo × 2` can't overflow). Use it to effectively disable a limit:

```cpp
st.setMinusLimit(-SpeedStepper::MAX_INT32_T);
st.setPlusLimit( SpeedStepper::MAX_INT32_T);
```

---

## 🧩 Canonical usage skeleton

```cpp
SpeedStepper m(STEP_PIN, DIR_PIN);

void setup() {
  m.setMaxSpeed(400);      // ceiling (≤ 1000)
  m.setAcceleration(1000); // ramp rate
  // optional: m.invertDirectionLogic();  // if it runs backwards
}

void loop() {
  // close your own position loop (à la trackAxis):
  int32_t err = target - m.getCurrentPosition();
  if (abs(err) < deadband) m.setSpeed(0);
  else                     m.setSpeed(constrain(KP * err, -MAX_SPEED, MAX_SPEED));

  m.run();   // <-- heartbeat, every iteration
}
```

---

## 🔒 Internal (NOT callable)
These are `private` — listed only so you know they exist and won't try to call them:
`runSpeed` · `internalSetSpeed` · `distanceToGo` · `setDir` · `computeNewSpeed` · `updateCurrentPosition` · `printComputeNewStepDebug` · `updateComputeTimes` · `printCurrentProfileStep`

> [!success] No shared state between instances
> Every field in `SpeedStepper.h` is **per-instance**. Two motors (`m1Pan`, `m2Tilt`) cannot affect each other in software — if one reacts to the other, it's **wiring**, not code.

---
*Related: [[main.cpp]] · [[config.h]] · [[Homing notes]] · [[UDP protocol]]*
