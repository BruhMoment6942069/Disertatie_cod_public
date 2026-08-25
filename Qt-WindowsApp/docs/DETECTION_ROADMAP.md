# 🎾 Detection Dev Roadmap — Tennis-Ball Tracking (Qt/OpenCV side)

> Doctrine (RoboCup-style): **fix the camera → calibrate by sampling → detect cheap → track a window → verify always.**
> Core principle: **Hough calibrates, color detects** — expensive/fragile methods run offline (calibration moment), cheap/robust methods run in the loop.
> Written 2026-07-07. Prereq (ESP slider pipeline, "Task 6") PASSED on hardware 2026-07-07.

## Settled design context (from the tracking debate — do not re-litigate)

- Detection runs on the **PC inside the Qt app** (OpenCV); ESP32 stays the fast dumb inner loop.
- Control = **velocity IBVS**: pixel error → `Kp·err` → **deg/s** on the wire (new `MSG_SPEED` message). Gain lives on the PC (live-tunable). User has prior proven results with this scheme (old Arduino rig used SpeedStepper too — same `setSpeed` semantics).
- Rails (all three, non-negotiable): **ESP watchdog** (~250 ms command silence → `setSpeed(0)`) for link death; **~5% pixel deadzone** (PC-side) so motors rest; **soft limits stay armed** (already built).
- **Sentry mode** (ball lost): PC-driven back-and-forth sweep via the same speed pipe. Sentry lives on PC (vision failure); watchdog lives on ESP (PC/link failure). Different layers, both needed.
- Controller policy: **P-only first**; add I only if chase-lag on a moving ball (then clamp + bleed in deadzone); add D only if latency oscillation (on smoothed error). Old Arduino PID ran at loop-rate on frame-rate data → D-spikes + integral inflation; new controller ticks once per frame by construction.
- **Microstepping 1/16 later** (SpeedStepper hard-caps 1000 steps/s ⇒ 1/16 ≈ 112°/s max, plenty vs ball's ~30–40°/s; 1/32 halves headroom for unusable resolution).
- Calibration = **sampling, not sliders** (the old HSV trackbars are explicitly banned by user experience).
- Cameras are poor (old AND new ones) → camera-settings discipline matters double.
- Thesis endgame: distance via stereo triangulation from the two gimbal pointing angles + residual pixel offset. deg/px calibration NOT needed for tracking but WILL be needed for triangulation. Ball is slow-ish; no Kalman initially.

---

## D0 — Plumbing 🔧
Get OpenCV into the Qt CMake/MinGW build. `VisionWorker` on a **QThread** (mirror the UdpClient threading pattern) grabbing frames; video widget in the app; FPS counter; camera **exposure/WB/gain lock** controls.
**✔ Done when:** live video at stable ~30 fps, UI responsive, exposure-lock toggle visibly freezes image brightness.
*Effort note: mostly build-system wrestling (vcpkg vs prebuilt OpenCV + `find_package`; CLion-bundled MinGW toolchain). Budget a full annoying day for CMake.*

## D1 — The harness 🎬 (the "get it done fast" secret)
Record button → short clips. Source switch: **camera OR clip file**, identical pipeline downstream. Pause / frame-step. Debug views: raw | mask | overlay.
Record the standard test set ONCE: ball near, far, window-light, lamp-light, moving, plus one clip with a yellow impostor object.
**✔ Done when:** replaying the same clip twice gives identical results. All tuning from here on happens on clips, never live.

## D2 — Calibration: geometry teaches color 🎯
Two calibration inputs, one output:
- **Auto (Hough):** "Calibrate" button → ball held still + prominent → `HoughCircles` on blurred gray → strongest circle **stable across ~5 consecutive frames** (stability = false-positive filter) → sample pixels inside the circle **minus a 20% edge margin** (edge pixels contaminated by background/shadow).
- **Manual (click/drag):** fallback, same sampling code path.
- **Output from pooled samples:** H median ± tolerance (tight — H discriminates), S/V loose percentiles (lighting moves S/V most) → `inRange` bounds; **plus** the 2D H-S histogram (backprojection food, built for free). Multi-sample across lighting zones; "reset samples" button; persist via `QSettings`.
Gotchas: widget→frame coordinate mapping for clicks (scaling/letterboxing); sample through the SAME color pipeline as runtime detection.
**✔ Done when:** new room = press Calibrate, hold ball, ~3 s, detection works. No sliders anywhere.

## D3 — Detection kernel + gates 🔍
`inRange` → one open + one close (NOT the old 32-pass morph chain) → contours (`RETR_EXTERNAL`, `CHAIN_APPROX_SIMPLE`) → gates: area window, **fill-ratio** vs `minEnclosingCircle` ≥ ~0.6 (better than approxPolyDP vertex counting), aspect 0.5–2 → **best single blob** (max area AFTER gating — never act on multiple) → centroid + radius.
**✔ Done when:** >95% detection rate on test-set frames where the ball is visible; ZERO false locks on the impostor clip. Measure with the harness.
*Escalation branch (only if D3 disappoints on clips): swap kernel to **histogram backprojection** using the D2 histogram — pipeline shape unchanged.*

## D4 — Lock-on state machine 🔒
`SEARCHING` (full-frame D3) → found → `LOCKED` (search ROI ~3 ball-diameters around last position) → miss counter → N misses → `SEARCHING`. Exponential smoothing on centroid. Runtime shape-verification = cheap fill-ratio per frame (NOT Hough — it already did its job in D2).
**✔ Done when:** FPS jumps in LOCKED; impostor can't steal a LOCKED ball (outside ROI = geometrically impossible); hiding the ball flips states correctly.

## D5 — Close the loop 🎛️
PC: smoothed centroid → error from frame center → 5% deadzone → `Kp·error` → deg/s → new **MSG_SPEED** message (wire stays little-endian; open sub-question: int32 centideg/s vs float32). ESP: `T_MOVEMENT` case executes streamed speed (native `setSpeed`) + **3-line watchdog**. PC: SEARCHING > a few seconds → sentry sweep through the same pipe.
**✔ Done when (live demo = thesis chapter):** ball wanders → gimbal centers it and rests silently; WiFi yanked → motors park; ball hidden → sentry sweeps; ball shown → re-lock and resume.

---

Old Python/Arduino code autopsy (what NOT to repeat): double `cap.read()` per loop (halved FPS); no-op stages (1×1 blur, Otsu on binary); 32-pass morphology that fused noise into the ball; acting on every passing contour instead of the best; global-gain "white balance" that can't fix hue casts (lock the camera instead); controller ticking at loop-rate on frame-rate data; unclamped integral; `!= 0` instead of a deadzone; unread limit-switch pins.
