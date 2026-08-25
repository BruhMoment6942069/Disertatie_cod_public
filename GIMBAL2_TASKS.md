# 🎯 Task sheet — scaling to TWO gimbals

> Written 2026-08-08, the night D5 closed the loop on gimbal #1 (`e382f91`).
> Order matters: **ESP first, PC second.** The PC should be written against a protocol
> that already exists on the robot, not the other way round.
> Test after every phase. Do not stack three untested changes and then debug.

---

## Phase A — ESP: from 2 motors to 4

Everything here is in `ProiectDisertatie/`. Good news: the firmware was written to scale.
`MOTOR_COUNT` is derived from the array size, homing already waits for
`homedCount == MOTOR_COUNT`, and the position packet already carries a count byte.

### A1. Pick the six new pins — do this BEFORE soldering

You need STEP, DIR and ENDSTOP for each of two new motors. Currently used: 26/13/15 (M1)
and 25/14/4 (M2).

**Hints / traps:**
- GPIO **34–39 are input-only** on ESP32. Fine for endstops, useless for STEP or DIR.
- Avoid **GPIO 6–11** entirely — they are wired to the flash chip.
- **0, 2, 12, 15** are strapping pins: if something holds them at the wrong level during
  reset, the board will not boot. You already use 15, so you know it can work — but do not
  add more of them if you can avoid it.
- Safe general-purpose choices: 16, 17, 18, 19, 21, 22, 23, 27, 32, 33.

**Done when:** six pins chosen and written into `config.h` as `M3_*` and `M4_*`, following
the existing M1/M2 block layout.

### A2. Add the mechanics and motion constants for M3 / M4

Copy the M1 and M2 blocks in `config.h`.

**Hints:**
- `M3_RANGE_DEG` / `M4_RANGE_DEG` are **physical degrees** — the new gimbal's real travel.
- `RANGE_STEPS` recomputes itself from `STEPS_PER_REV`; do not hand-calculate it.
- Speeds and accelerations are in **steps per second**, so they are already in your new
  1/16 microstepped units. Copy M1/M2's values as a starting point.
- `DIR_M3` / `DIR_M4` are homing directions and depend on how you physically mount the new
  gimbal. Expect to flip a sign after the first homing attempt — that is normal.

### A3. Grow the arrays

In `src/main.cpp`:

- Two more `SpeedStepper` objects next to `m1Pan` / `m1Tilt`.
- Two more entries in the `motors[]` initialiser. `MOTOR_COUNT` updates itself from
  `sizeof(motors)/sizeof(motors[0])` — do not hardcode 4 anywhere.
- `g_speed[2]` → `g_speed[4]`, and `g_target[2]` → `g_target[4]`.

**The trap:** `stepperTask` calls `motorRun` twice by hand:

```cpp
motorRun(motors[0], g_target[0], g_speed[0]);
motorRun(motors[1], g_target[1], g_speed[1]);
```

Turn that into a loop over `MOTOR_COUNT`. Same for the two `.run()` calls, and for the
position array in the streaming block — that one is hardcoded to two entries too.

**Done when:** it compiles, and all four axes home in sequence on boot.

### A4. Extend `MSG_SPEED` to carry four values

Right now the packet is `[id][pan int32][tilt int32]` = 9 bytes.

**Hint — copy the pattern you already invented.** `MSG_MOTOR_POS` sends
`[id][count][value × count][homed]`. Do the same here: `[id][count][value × count]`. Then
the parser is a loop, and going to six motors later costs nothing.

**Trap:** the ESP's receive buffer is `uint8_t buf[16]`. Four int32 plus id plus count is
**18 bytes** — it will not fit. Grow it.

**Done when:** the ESP prints four sensible deg/s values when the PC sends them, and the
length check rejects short packets instead of reading past the end.

### A5. Test with drivers unplugged

Same discipline as tonight: watch serial, confirm all four axes home, confirm the
`[rx]` counter climbs, confirm the watchdog still zeroes the command when you close the app.
**Only then** power the drivers.

---

## Phase B — PC: a second vision pipeline

Everything here is in `Qt-WindowsApp/`.

### B1. Give `VisionWorker` an identity

This is the one that will silently corrupt your work if you skip it.

```cpp
QSettings _settings = QSettings("../calibration.ini", QSettings::IniFormat);
```

Hardcoded, in `VisionWorker.h`. Two workers share one file, so calibrating camera B
**destroys camera A's bounds** — and they genuinely need different values, because the two
cameras see the ball under different lighting angles.

**Hint:** give the constructor an id or a name, and build the path from it —
`../calibration_0.ini`, `../calibration_1.ini`. The member cannot stay a default-initialised
field once it depends on a constructor argument; move it into the constructor's
initialiser list.

**Also check:** the recording path (`../clips`) is shared. Filenames are timestamped so they
probably will not collide, but include the camera id in the name so you can tell the two
apart later.

**Done when:** each worker reads and writes its own ini, and calibrating one does not move
the other's numbers.

### B2. Put an id on `ballTracked`

```cpp
void ballTracked(bool locked, double errX, double errY);
```

The controller cannot tell which gimbal that came from.

**Hint:** add the id as the first parameter. Prefer that over `QObject::sender()` — it keeps
the routing explicit and it is much easier to read in six months.

Then `handleBallTracked` routes to the right pair of motors: gimbal 0 drives speeds 0 and 1,
gimbal 1 drives speeds 2 and 3.

**Design question to decide:** do you send one `MSG_SPEED` with all four values, or one per
gimbal? One packet is cleaner on the wire but means the controller must hold the last known
speed of the *other* gimbal. Holding a small `qint32 _speeds[4]` member and sending the whole
array whenever either camera updates is the simplest thing that works.

### B3. Second worker + thread

`MainController` currently creates one `VisionWorker` on one `QThread`.

**Hints:**
- Two workers, two threads. Do not put both on one thread — a slow frame in one would stall
  the other.
- The connect block is about to become a copy-paste field. Consider a small private helper
  that takes a worker and an id and wires it up, so the two are guaranteed identical.
- Remember `captureStartRequested` is a signal into a specific worker. With two, a broadcast
  signal reaches both. This is the moment that pattern stops working.

### B4. Second video view

`pageVideo` holds `videoView`. You need a second one for camera B.

**Hint:** the UI already has four coordinate labels wired for four motors
(`lblM1X1`, `lblM1Y1`, `lblM2X2`, `lblM2Y2`, reading indices 0–3), so the motor side of the
display is already built for this.

Keep it simple: two views side by side on the tracking page. Do not build a fancy layout
before the pipeline works.

### B5. Camera enumeration and USB bandwidth

**The gotcha that looks like a code bug:** two USB cameras on the same host controller can
exceed bandwidth on uncompressed streams. Symptoms are the second camera failing to open, or
both dropping to a terrible framerate.

**Fix if it happens:** force MJPEG with `CAP_PROP_FOURCC` and
`cv::VideoWriter::fourcc('M','J','P','G')`, or plug the cameras into physically different
USB controllers (usually front vs rear ports on a desktop).

---

## Phase C — integration

**C1.** Both cameras detecting, both masks clean, each calibrated **separately under the
lighting you will actually demo in**.

**C2.** Both gimbals tracking the same ball at once. Expect to fix at least one direction
sign — test each axis with the ball held still, exactly as you did tonight.

**C3.** Commit. Then update `HANDOFF.md`.

---

## What comes after (do not start until C passes)

1. **Extrinsic calibration** — measure the baseline, and establish what each gimbal's homed
   zero means in the shared frame. This is the accuracy-determining step of the whole thesis.
2. **Triangulation math** — each gimbal's angles give a 3D ray; the two rays never exactly
   intersect, so take the closest point between the skew lines and use the midpoint.
   Roughly 20 lines. **Testable with fake angles before either gimbal is involved** — and
   worth writing during any gap where you are waiting on hardware.

---

## Reminders from tonight

- **Check that your instrumentation measures the thing you are debugging.** Three separate
  times tonight a readout lied: dead code after a `break`, a debug view rendering something
  the pipeline ignores, and a log line printing a call that had silently no-op'd.
- **A velocity command is a standing order.** Anything new that commands speed needs the same
  stale-command watchdog thinking.
- **Fix orientation in one place.** The frame flip sets the sign of the tracking error; do not
  also negate the gain, or they cancel and you get a runaway with no obvious cause.
- Anything measured in **steps** scales with microstepping; anything in **degrees** does not.
