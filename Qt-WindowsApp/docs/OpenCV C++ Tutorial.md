---
title: OpenCV C++ — Field Guide
aliases: [OpenCV, OpenCV Tutorial, Vision Guide]
tags: [opencv, cpp, qt, vision, tracking, disertatie, reference]
created: 2026-07-07
scope: everything needed for the detection roadmap D0–D5, nothing more
---

# 👁️ OpenCV C++ — Field Guide

> [!abstract] What is this?
> A curated OpenCV-for-C++ reference for the **tennis-ball detection pipeline** ([[DETECTION_ROADMAP]]). Not the whole library — just the ~15% you'll actually touch, with the traps marked. Written for someone coming from Python OpenCV: the concepts transfer 1:1, the *memory model* does not.

---

## 🧠 The mental model (read this first)

> [!important] `cv::Mat` is a smart pointer, not a matrix
> A `Mat` is a small **header** (rows, cols, type, stride) plus a **ref-counted pointer** to pixel data. Copying a `Mat` copies the *header only* — both objects share the same pixels. This is the #1 source of "why did my image change itself" bugs.
>
> ```cpp
> cv::Mat a = frame;      // SHALLOW — a and frame share pixels
> cv::Mat b = frame.clone(); // DEEP — b owns its own pixels
> ```
> Rule of thumb: **crossing a thread boundary or storing for later** ⇒ `clone()`. Passing down a call chain for immediate use ⇒ shallow is fine (and fast).

> [!warning] The three Python-to-C++ surprises
> 1. **BGR, not RGB.** Same as Python, but it bites again when you hand pixels to Qt (which thinks RGB).
> 2. **Hue is 0–179**, not 0–255 (it's degrees/2 to fit a byte). S and V are 0–255. Your ball-yellow lives around H ≈ 20–30.
> 3. **Row comes first:** `at<>(row, col)` is (y, x). Every coordinate bug you will ever have is this line.

---

## 📇 Quick index

| Phase | What you need |
|---|---|
| [[#🔧 Build (CMake)]] | `find_package`, linking |
| [[#🎥 Capture & recording]] | `VideoCapture`, property locks, `VideoWriter` |
| [[#🎨 Color & thresholding]] | `cvtColor`, `inRange` |
| [[#🧹 Morphology]] | `getStructuringElement`, `morphologyEx` |
| [[#🔍 Contours & shape]] | `findContours`, gates, centroid |
| [[#⭕ Hough circles]] | calibration-time ball finder |
| [[#📊 Histogram & backprojection]] | the D2/D3-escalation kernel |
| [[#🖼️ OpenCV ↔ Qt]] | `Mat`→`QImage`, threading rules |
| [[#🧩 Canonical pipeline skeleton]] | D3 in one page |
| [[#🪤 Gotcha table]] | print this one |

---

## 🔧 Build (CMake)

```cmake
find_package(OpenCV REQUIRED)                 # needs OpenCV_DIR if not system-wide
target_link_libraries(Qt-WindowsApp PRIVATE ${OpenCV_LIBS})
# include dirs come with the imported targets in modern OpenCV; if not:
# target_include_directories(Qt-WindowsApp PRIVATE ${OpenCV_INCLUDE_DIRS})
```

```cpp
#include <opencv2/opencv.hpp>   // the everything-header; fine for app code
```

> [!warning] MinGW reality check
> Prebuilt OpenCV releases for Windows ship **MSVC** binaries — they will not link against your CLion MinGW toolchain. Options: build OpenCV once from source with your MinGW (slow but clean), or use a package manager that builds for your triplet. Mixing MSVC-built OpenCV with MinGW objects fails at *link* time with walls of undefined references — recognize that wall for what it is.

---

## 🎥 Capture & recording

### Opening a camera
```cpp
cv::VideoCapture cap(0, cv::CAP_DSHOW);   // Windows: DirectShow backend
if (!cap.isOpened()) { /* bail loudly */ }
cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
cap.set(cv::CAP_PROP_FPS,          30);
```

> [!tip] Backend matters on Windows
> `CAP_DSHOW` and `CAP_MSMF` expose *different* property sets on cheap cams. If exposure lock silently does nothing on one backend, try the other before blaming the camera.

### The camera-discipline block (Layer 0 of the roadmap)
```cpp
cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25); // DSHOW dialect: 0.25 = manual, 0.75 = auto
cap.set(cv::CAP_PROP_EXPOSURE,      -6);   // log2 scale-ish; tune by eye once
cap.set(cv::CAP_PROP_AUTO_WB,       0);
cap.set(cv::CAP_PROP_WB_TEMPERATURE, 4500);
```

> [!bug] Cheap cameras lie
> `set()` returns `true` and then the camera ignores you. **Verify with your eyes** (image must stop breathing when lighting changes) — the return value proves nothing. If a camera truly can't lock exposure, that's a camera-selection criterion.

### Reading frames
```cpp
cv::Mat frame;
if (!cap.read(frame)) { /* stream ended / device lost */ }
```
One `read()` per loop iteration. *(The old Python rig called it twice per loop and silently halved its FPS — never again.)*

### Recording clips (the D1 harness)
```cpp
cv::VideoWriter rec("clip.avi",
                    cv::VideoWriter::fourcc('M','J','P','G'),  // safest codec on bare Windows
                    30, cv::Size(640, 480));
rec.write(frame);
```
`VideoCapture cap("clip.avi")` plays it back through the *identical* pipeline — that's the whole harness trick.

---

## 🎨 Color & thresholding

```cpp
cv::Mat hsv;
cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

cv::Mat mask;
cv::inRange(hsv, cv::Scalar(lowH, lowS, lowV),
                 cv::Scalar(highH, highS, highV), mask);   // mask: CV_8U, 0 or 255
```

> [!tip] Range philosophy (from the calibration design)
> **H tight** (median ± ~8 — it's the channel that actually discriminates), **S/V loose** (low percentile → 255 — lighting moves these the most). The ranges come from *sampling the ball*, never from hand-tuned sliders. See [[DETECTION_ROADMAP#D2 — Calibration: geometry teaches color 🎯]].

Splitting channels when you need just one: `cv::split(hsv, chans)` → `chans[0]` is H.

---

## 🧹 Morphology

```cpp
cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kernel);  // erode→dilate: kills specks
cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);  // dilate→erode: fills holes
```

> [!important] Two composite ops. That's it.
> `MORPH_OPEN` then `MORPH_CLOSE`, one elliptical kernel, 1–2 iterations. The old project ran **32 raw passes** with mixed kernels — it fused noise *into* the ball and distorted the shape its own gates then rejected. Order matters: open **first** so noise dies before anything dilates it into the blob.

---

## 🔍 Contours & shape

```cpp
std::vector<std::vector<cv::Point>> contours;
cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
```
- `RETR_EXTERNAL` — outer contours only (holes inside the ball are noise, not structure)
- `CHAIN_APPROX_SIMPLE` — compresses straight runs; you don't need every pixel

### The gate stack (D3)
```cpp
double area = cv::contourArea(c);

cv::Point2f center; float radius;
cv::minEnclosingCircle(c, center, radius);
double fillRatio = area / (CV_PI * radius * radius);   // 1.0 = perfect disc

cv::Rect box = cv::boundingRect(c);
double aspect = static_cast<double>(box.width) / box.height;
```

| Gate | Pass condition | Rejects |
|---|---|---|
| area window | `minA < area < maxA` | specks, walls |
| fill ratio | `>= ~0.6` | crescents, blobs with bites |
| aspect | `0.5 – 2.0` | stripes, arms |

> [!important] Then pick ONE winner
> `std::max_element` over the *survivors* by area. The old code acted on **every** passing contour — two yellow blobs = two commands per frame fighting at the motor.

### Centroid — two ways
```cpp
// cheap and fine for a disc:
cv::Point2f c1 = center;                       // from minEnclosingCircle
// classic (weights by actual pixels):
cv::Moments m = cv::moments(cont);
cv::Point2f c2(m.m10 / m.m00, m.m01 / m.m00);  // guard m.m00 != 0
```

---

## ⭕ Hough circles

> [!note] Calibration-time only — "Hough calibrates, color detects"
> Runtime Hough is slow and motion-blur-fragile. Its one job: find the *static, prominent* ball during D2 calibration so its pixels can be sampled. See the roadmap for the stability rule (same circle ~5 consecutive frames).

```cpp
cv::Mat gray, blurred;
cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
cv::GaussianBlur(gray, blurred, cv::Size(9,9), 2);      // Hough NEEDS pre-blur

std::vector<cv::Vec3f> circles;                          // (x, y, r)
cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT,
                 1,      // dp: accumulator resolution (1 = full)
                 100,    // minDist between centers — large = "one ball max"
                 100,    // param1: Canny high threshold
                 30,     // param2: accumulator votes — LOWER = more (junk) circles
                 10, 150 // radius bounds — tie to plausible ball sizes
);
```
`param2` is the knob that matters: too high → finds nothing; too low → finds circles in wood grain.

Sampling inside the found circle, minus the contaminated rim:
```cpp
cv::Mat sampleMask = cv::Mat::zeros(frame.size(), CV_8U);
cv::circle(sampleMask, cv::Point(cvRound(x), cvRound(y)),
           cvRound(r * 0.8), 255, cv::FILLED);           // 20% edge margin
```

---

## 📊 Histogram & backprojection

Built for free from D2's samples; becomes the detection kernel only if `inRange` disappoints (D3 escalation).

```cpp
// build once, from sample pixels (hsv + sampleMask):
int   channels[] = {0, 1};                      // H and S; V deliberately ignored
int   histSize[] = {30, 32};
float hRange[] = {0, 180}, sRange[] = {0, 256};
const float* ranges[] = {hRange, sRange};
cv::Mat hist;
cv::calcHist(&hsv, 1, channels, sampleMask, hist, 2, histSize, ranges);
cv::normalize(hist, hist, 0, 255, cv::NORM_MINMAX);

// per frame:
cv::Mat prob;
cv::calcBackProject(&hsv, 1, channels, hist, prob, ranges);
cv::threshold(prob, mask, 50, 255, cv::THRESH_BINARY);   // then morphology as usual
```
Everything downstream (morphology → contours → gates) is unchanged — that's the point of swappable kernels.

---

## 🖼️ OpenCV ↔ Qt

### Mat → QImage (display path)
```cpp
QImage img(mat.data, mat.cols, mat.rows,
           static_cast<int>(mat.step),
           QImage::Format_BGR888);          // Qt ≥ 5.14 — no channel swap needed
QPixmap pix = QPixmap::fromImage(img.copy());   // .copy() — see the trap below
```

> [!danger] The lifetime trap
> That `QImage` constructor does **not copy pixels** — it wraps `mat`'s buffer. If `mat` is reassigned next frame while the QImage is still queued for painting, you render garbage (or crash). `img.copy()` before the image leaves the function. Pixel copies at 640×480×30 fps are nothing.

### Threading rules (VisionWorker ↔ GUI)
- `cv::Mat`'s ref-count is **not** a thread-safety guarantee. Anything emitted across threads gets a `clone()` **first**.
- A queued Qt signal carrying `cv::Mat` needs the type registered once:
  ```cpp
  Q_DECLARE_METATYPE(cv::Mat)              // header, after includes
  qRegisterMetaType<cv::Mat>();            // once at startup
  ```
- Better still: emit *results* (`ballFound(QPointF err, float radius)`, `ballLost()`) plus a display frame — the GUI never touches raw pipeline internals. Named slots, as always.

> [!warning] No `imshow`/`waitKey` inside the Qt app
> `cv::waitKey` runs OpenCV's own event pump — inside a Qt app the two event loops fight. All display goes through your Qt widget path. (`imshow` is fine in throwaway CLI test tools.)

---

## 🧩 Canonical pipeline skeleton

```cpp
// per frame, inside VisionWorker — D3 in one page
cv::Mat hsv, mask;
cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
cv::inRange(hsv, lower, upper, mask);                    // bounds from D2 sampling

cv::morphologyEx(mask, mask, cv::MORPH_OPEN,  kernel);
cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

std::vector<std::vector<cv::Point>> contours;
cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

const std::vector<cv::Point>* best = nullptr;            // gate, then pick ONE
double bestArea = 0;
for (const auto& c : contours) {
    if (!passesGates(c)) continue;                       // area / fill / aspect
    double a = cv::contourArea(c);
    if (a > bestArea) { bestArea = a; best = &c; }
}

if (best) { /* centroid -> smooth -> error -> deadzone -> deg/s */ }
else      { /* miss counter -> SEARCHING -> sentry */ }
```

---

## 🪤 Gotcha table

| Trap | Symptom | Fix |
|---|---|---|
| shallow `Mat` copy | image "changes itself" | `clone()` across threads/storage |
| BGR vs RGB | Smurf-colored video in Qt | `Format_BGR888` (or `rgbSwapped()`) |
| H is 0–179 | Python-era H values ×2 too big | divide old H numbers by 2… or better: resample, don't port numbers |
| `at<>(x, y)` | reads garbage / crashes | it's `at<>(row, col)` = (y, x) |
| QImage wraps, doesn't copy | flickering/garbage frames | `.copy()` before it leaves scope |
| `set()` returns true, camera ignores it | tuning rots mid-session | verify locks by eye; try other backend |
| `waitKey` in Qt app | frozen/psychotic UI | Qt widgets only |
| MSVC OpenCV + MinGW app | undefined-reference wall at link | build OpenCV for MinGW |
| Hough without blur | circles everywhere / nowhere | `GaussianBlur` first, tune `param2` |
| morphology before open | noise fused into blob | `OPEN` first, then `CLOSE`, 2 ops total |

---
*Related: [[DETECTION_ROADMAP]] · [[SpeedStepper API]] · [[HANDOFF]]*
