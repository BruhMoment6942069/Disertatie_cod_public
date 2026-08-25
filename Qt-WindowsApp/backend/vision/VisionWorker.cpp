//
// Created by Andrei on 08.07.2026.
//

#include "VisionWorker.h"
#include <QDebug>
#include <cmath>
#include <QDir>
#include <QDateTime>
#include <opencv2/imgproc.hpp>
#include <QSettings>

static constexpr int MISS_LIMIT = 10;

// The aim point, drawn so a human can see what the loop is actually steering to. errX and
// errY are measured from the exact frame centre, so this crosshair IS the target: whatever
// the ball's red centre dot sits on top of, errX and errY are zero.
//
// Broken in the middle on purpose. A solid cross covers the one pixel you most need to
// look at, which is the gap between the ball's centre and the aim point. Magenta because
// green, cyan and red are already spent on the ball, the ROI and the ball centre.
static void drawAimPoint(cv::Mat& frame) {
    const int cx  = frame.cols / 2;
    const int cy  = frame.rows / 2;
    const int arm = 22;   // px from centre to the outer tip of each stroke
    const int gap = 6;    // px of clear space either side of dead centre
    const cv::Scalar colour(255, 0, 255);

    cv::line(frame, cv::Point(cx - arm, cy), cv::Point(cx - gap, cy), colour, 1);
    cv::line(frame, cv::Point(cx + gap, cy), cv::Point(cx + arm, cy), colour, 1);
    cv::line(frame, cv::Point(cx, cy - arm), cv::Point(cx, cy - gap), colour, 1);
    cv::line(frame, cv::Point(cx, cy + gap), cv::Point(cx, cy + arm), colour, 1);
}

// Half-width of the hue window taken around the ball's median hue.
// Measured 2026-08-06 under warm bulb: the whole scene crams into H 6-16, so the old
// +/-8 window spanned the ball (H 14) AND background points at 6.5 / 9.0 / 12.2 -> flood.
// +/-4 rejects two of those three outright; the S floor below handles the third.
// If the ball drops out under daylight (where hue is well spread), widen this back toward 8.
static constexpr int HUE_HALF_WIDTH = 4;

// Morphology kernels, deliberately asymmetric.
// OPEN (erode->dilate) is the speckle killer and must stay SMALL: it eats `size/2` pixels off
// every edge, so a large one thins the ball and can split an already-patchy blob into fragments.
// CLOSE (dilate->erode) is the hole filler and wants to be LARGE: the tightened colour bounds
// punch gaps in the ball, and a holey blob fails the fill-ratio gate (area / pi*r^2 < 0.6) in
// detectBall even though the ball is right there. CLOSE welds those patches back into one disc.
static constexpr int MORPH_OPEN_SIZE  = 3;
static constexpr int MORPH_CLOSE_SIZE = 9;
// Final regrow pass, applied AFTER the speckle kill so it grows the ball back rather than
// inflating noise. Set to 0 or 1 to disable and A/B it.
static constexpr int MORPH_DILATE_SIZE = 5;

// Gaussian blur applied before thresholding. Must be ODD. Bigger = smoother mask but a
// small/distant ball starts bleeding into its surroundings.
static constexpr int BLUR_SIZE = 11;

// Measure the convex hull of each candidate instead of its raw outline.
// The ball's white seams and logo are not ball-coloured, so they fail inRange and bite concave
// notches out of the mask, dragging the fill ratio down on a ball that is genuinely there.
// The hull bridges those dents. It does NOT force a circle - a hand's hull is still an
// elongated hand with a poor fill ratio - so impostor rejection below keeps working.
// Flip to false to A/B against the raw contour.
static constexpr bool USE_CONVEX_HULL = true;

// Camera orientation. Applied to the raw frame BEFORE anything else reads it, so the mask,
// click sampling, the overlay, recorded clips and the tracking error all agree on one
// orientation - a flip applied later would leave those disagreeing.
// FLIP_CODE follows cv::flip: 1 = horizontal (mirror), 0 = vertical, -1 = both (180 deg).
//
// This sets DISPLAY AND DETECTION orientation only. Loop polarity lives in one place:
// AXIS_SIGN in MainController.cpp. Do not encode "the gimbal runs the wrong way" as a flip
// here - that buries control polarity inside display orientation, and the triangulation math
// later needs image coordinates in a known relation to the physical camera.
//
// But they ARE coupled, so: changing this HORIZONTALLY inverts errX and the pan entries of
// AXIS_SIGN must be re-measured; VERTICALLY inverts errY and the tilt entries must be.
static constexpr bool FLIP_ENABLED = true;
static constexpr int  FLIP_CODE    = 1;


VisionWorker::VisionWorker(int camId, QObject *parent)
    : QObject(parent),
      _camId(camId),
      _settings(QString("../calibration_%1.ini").arg(camId), QSettings::IniFormat){

    _grabTimer = new QTimer(this);
    connect(_grabTimer, &QTimer::timeout, this, &VisionWorker::grabFrame);
    static const char* channels[3] = {"h", "s", "v"};
    for (int i = 0; i < 3; ++i) {
        _lower[i] = _settings.value(QString("calib/%1Low").arg(channels[i]), 0.0).toDouble();
        _upper[i] = _settings.value(QString("calib/%1High").arg(channels[i]), 0.0).toDouble();
    }
    qDebug() << "[calib] loaded H" << _lower[0] << "-" << _upper[0] << " S floor" << _lower[1];

}

void VisionWorker::startCapture(int camId, int cameraIndex) {
    if (camId != _camId) {
        return;
    }
    _cap.open(cameraIndex, cv::CAP_DSHOW);
    if (!_cap.isOpened()) {
        emit cameraFailed("Kaboom Kblaw");
        return;
    }
    _isFileSource = false;
    _cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    _cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    _cap.set(cv::CAP_PROP_FPS, 30);
    _cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.75);
    _cap.set(cv::CAP_PROP_AUTO_WB, 1);
    _elapsed.start();
    _grabTimer->start(0);

}

void VisionWorker::stopCapture(int camId) {
    if (camId != _camId) {
        return;
    }
    _grabTimer->stop();
    _cap.release();
    _writer.release();
}


void VisionWorker::grabFrame() {
    ++_frameCount;
    double fps = 0.0;
    cv::Mat frame;
    _cap.read(frame);
    if (frame.empty()) {
        if (_isFileSource) {
            _cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            return;
        }
        _grabTimer->stop();
        emit cameraFailed("frame empty");
        return;
    }
    if (FLIP_ENABLED) {
        cv::flip(frame, frame, FLIP_CODE);
    }
    _lastRaw = frame.clone();
    if (_writer.isOpened()) {
        _writer.write(frame);
    }
    if (_autoCalib) {
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(9,9),2);
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT,
            1,            // dp
            gray.rows,    // minDist > frame height => at most ONE (strongest) circle
            100,          // param1: Canny high threshold
            30,           // param2: LOOSE confidence — the ball's edge on a gray wall is only moderate
            25,           // minRadius
            85);          // maxRadius: THIS is the forehead gate — ball ~59px, a forehead circle is bigger
        if (circles.empty()) {
            if (_stableCount > 0) --_stableCount;    // brief Hough dropout: decay, don't wipe the streak
        } else {
            cv::Vec3f c = circles[0];
            float dx = c[0] - _lastCircle[0];
            float dy = c[1] - _lastCircle[1];

            if (std::hypot(dx, dy) < 15.0 && std::abs(c[2] - _lastCircle[2]) < 0.3f * _lastCircle[2]) {
                ++_stableCount;                       // "held roughly still by a hand", not clamped in a vise
            } else {
                _stableCount = 1;
            }
            _lastCircle = circles[0];
        }
        if (_stableCount >= 5) {
            _poolH.clear();
            _poolS.clear();
            _poolV.clear();
            cv::Mat mask = cv::Mat::zeros(frame.size(), CV_8U);
            cv::circle(mask, cv::Point(cvRound(_lastCircle[0]), cvRound(_lastCircle[1])),
                cvRound(_lastCircle[2] * 0.8f),
                255,
                cv::FILLED);
            cv::Mat hsv;
            cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
            samplePixels(hsv, mask);
            computeBounds();
            _autoCalib = false;
            emit autoCalibrateFinished(_camId);
            qDebug() << "[calib] auto-locked at r =" << _lastCircle[2];
        }
    }
    updateTracker(frame);
    double errX = (_trackCenter.x - frame.cols / 2.0) / (frame.cols / 2.0);
    double errY = (_trackCenter.y - frame.rows / 2.0) / (frame.rows / 2.0);
    emit ballTracked(_camId, _trackState == TrackState::Locked, errX, errY);

    switch (_debugView) {
        case 1: {
            cv::Mat mask = buildMask(frame);
            // Show only what detectBall actually analyses. When Locked the search is confined
            // to _searchRoi, so specks outside it are invisible to the detector - rendering the
            // whole-frame mask made harmless background noise look like a threat.
            if (_trackState == TrackState::Locked
                && _searchRoi.width > 0 && _searchRoi.height > 0) {
                cv::Mat gated = cv::Mat::zeros(mask.size(), CV_8U);
                mask(_searchRoi).copyTo(gated(_searchRoi));
                mask = gated;
                cv::rectangle(mask, _searchRoi, cv::Scalar(128), 1);   // grey = the ROI border
            }
            QImage img(mask.data, mask.cols, mask.rows, static_cast<int>(mask.step), QImage::Format_Grayscale8);
            emit frameReady(_camId, img.copy());
            break;
        }

        case 2: {
            if (_autoCalib) {
                if (_lastCircle[2] > 0) {
                    cv::circle(frame, cv::Point(cvRound(_lastCircle[0]), cvRound(_lastCircle[1])),cvRound(_lastCircle[2]),
                        cv::Scalar(0,255,0),
                        2);
                }
            } else if ( _trackState == TrackState::Locked) {
                cv::circle(frame, cv::Point(cvRound(_trackCenter.x), cvRound(_trackCenter.y)),
                        cvRound(_trackRadius), cv::Scalar(0,255,0), 2);
                cv::circle(frame, cv::Point(cvRound(_trackCenter.x), cvRound(_trackCenter.y)), 3,
                    cv::Scalar(0,0,255), cv::FILLED);
                // the box detectBall is actually restricted to while Locked
                cv::rectangle(frame, _searchRoi, cv::Scalar(255, 255, 0), 1);
            }
            // LAST, and outside the lock branch on purpose. The aim point does not depend
            // on having a lock, and it is most useful when there ISN'T one - it shows you
            // where the head is looking while you walk the ball into view.
            drawAimPoint(frame);
            QImage img(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
            emit frameReady(_camId, img.copy());
            break;

        }
            default: {
            QImage img(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_BGR888);
            emit frameReady(_camId, img.copy());
            break;
        }
    }

    qint64 ms = _elapsed.elapsed();
    if (ms >= 1000) {
        fps = static_cast<double>(_frameCount * 1000.0 / ms);
        emit fpsUpdated(_camId, fps);
        _elapsed.restart();
        _frameCount = 0;
    }

}

void VisionWorker::samplePixels(const cv::Mat &hsv, const cv::Mat &mask) {
    for (int r = 0; r <hsv.rows; ++r) {
        for (int c = 0; c <hsv.cols; ++c) {
            if (mask.at<uchar>(r, c) == 0) {
                continue;
            }
            cv::Vec3b px = hsv.at<cv::Vec3b>(r, c);
            _poolH.push_back(px[0]);
            _poolS.push_back(px[1]);
            _poolV.push_back(px[2]);
        }
    }
}

cv::Mat VisionWorker::buildMask(const cv::Mat &frame) {
    // Blur BEFORE thresholding: inRange decides per pixel, so raw sensor noise (bad in this
    // low warm light) speckles the mask and punches holes in the ball. Averaging neighbours
    // first makes the ball a coherent region before it is ever thresholded.
    // Blur in BGR, not in HSV - hue is a circular quantity and averaging it across the 180/0
    // wrap produces nonsense.
    cv::Mat blurred, hsv, mask;
    cv::GaussianBlur(frame, blurred, cv::Size(BLUR_SIZE, BLUR_SIZE), 0);
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, _lower, _upper, mask);

    // small OPEN first (kill speckles without thinning the ball), then big CLOSE (weld the
    // ball's patches back into a solid disc so the fill-ratio gate can recognise it).
    cv::Mat kOpen = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(MORPH_OPEN_SIZE, MORPH_OPEN_SIZE));
    cv::Mat kClose = cv::getStructuringElement(cv::MORPH_ELLIPSE,
        cv::Size(MORPH_CLOSE_SIZE, MORPH_CLOSE_SIZE));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kOpen);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kClose);

    // Reconstruction pass: OPEN shaved ~MORPH_OPEN_SIZE/2 px off the rim and the blur washed
    // out another ring of edge pixels - proportionally brutal once the ball is far away and
    // small. A final dilate grows that back. It runs LAST, after the speckle kill, so it is
    // regrowing the ball rather than inflating noise.
    // The radius it returns is slightly generous, which is harmless here: distance comes from
    // the vergence angle between the two gimbals, never from apparent size, and a symmetric
    // dilate does not move the centre - the one quantity that has to stay accurate.
    if (MORPH_DILATE_SIZE > 1) {
        cv::Mat kDilate = cv::getStructuringElement(cv::MORPH_ELLIPSE,
            cv::Size(MORPH_DILATE_SIZE, MORPH_DILATE_SIZE));
        cv::dilate(mask, mask, kDilate);
    }
    return mask;
}


void VisionWorker::setExposureLock(bool locked) {

    if (!_cap.isOpened()) {
        return;
    }
    if (locked) {

        cv::Mat frame;
        _cap.read(frame);
        if (frame.empty()) {
            return;
        }
        cv::Scalar m = cv::mean(frame);
        double autoMean = (m[0] + m[1] + m[2]) / 3.0;
        _cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);
        _cap.set(cv::CAP_PROP_AUTO_WB, 0);
        double exposure = _cap.get(cv::CAP_PROP_EXPOSURE);

        if (exposure < -11.0 || exposure > -2.0) {
            exposure = -6.0;
        }
            _cap.set(cv::CAP_PROP_EXPOSURE, exposure);

        for (int attempt = 0; attempt < 3; ++attempt) {
            for (int skip = 0; skip < 3; ++skip) {
                _cap.read(frame);
            }
            _cap.read(frame);
            if (frame.empty()) {
                break;
            }
            m = cv::mean(frame);
            double manualMean = (m[0] + m[1] + m[2]) / 3.0;
            if (manualMean < 1.0) {
                manualMean = 1.0;
            }

            double ratio = autoMean / manualMean;
            if (ratio > 0.7 && ratio < 1.5) {
                qDebug() << "[vision] converged: exposure" << exposure << "in" << attempt + 1 << "tries";
                break;
            }
            exposure += std::log2(ratio);
            exposure = std::lround(exposure);
            exposure = std::clamp(exposure, -11.0, -2.0);
            _cap.set(cv::CAP_PROP_EXPOSURE, exposure);
            qDebug() << "[vision] correcting: auto" << autoMean << "vs manual" << manualMean << "->exposure" << exposure;
        }

        double wbTemp = _cap.get(cv::CAP_PROP_WB_TEMPERATURE);
        if (wbTemp >= 2800.0 && wbTemp <= 6500.0) {
            _cap.set(cv::CAP_PROP_WB_TEMPERATURE, wbTemp);
        } else {
            _cap.set(cv::CAP_PROP_WB_TEMPERATURE, 3000.0);
        }
    } else {
        _cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.75);
        _cap.set(cv::CAP_PROP_AUTO_WB, 1);
    }
}

void VisionWorker::setRecording(bool recording) {
    if (!_cap.isOpened()) {

        return;
    }

    if (recording) {
        QDir("../clips").mkpath(".");
        QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString filename = QString("../clips/rec_cam%1_%2.avi").arg(_camId).arg(stamp);

        _writer.open(filename.toStdString(),
            cv::VideoWriter::fourcc('M','J','P','G'),
            30.0,
            cv::Size(static_cast<int>(_cap.get(cv::CAP_PROP_FRAME_WIDTH)),
                static_cast<int>(_cap.get(cv::CAP_PROP_FRAME_HEIGHT))),
            true);
        if (!_writer.isOpened()) {
            qDebug() << "[vision] couldn't open video";
            return;
        }
        qDebug() << "[vision] recording" << filename;
    } else {
        _writer.release();
    }
}

void VisionWorker::startPlayback(int camId, const QString &path) {
    if (camId != _camId) {
        return;
    }
    _cap.open(path.toStdString());
    if (!_cap.isOpened()) {
        emit cameraFailed("clip won't open");
        return;
    }
    _isFileSource = true;
    double fps = _cap.get(cv::CAP_PROP_FPS);

    if (fps <= 0.0) {
        fps = 30.0;
    }
    _elapsed.start();
    _grabTimer->start(std::lround(1000.0 / fps));
}

void VisionWorker::setDebugView(int view) {
    _debugView = view;
    qDebug() << "[vision] debugView" << view;
}

void VisionWorker::sampleAt(int camId, QPoint p) {
    // addressed: a click on one view must not sample the same coordinates out of the
    // OTHER camera's frame and write them into its calibration pool.
    if (camId != _camId) {
        return;
    }
    if (_lastRaw.empty()) {
        return;
    }
    cv::Mat hsv;
    cv::cvtColor(_lastRaw, hsv, cv::COLOR_BGR2HSV);

    cv::Vec3b px = hsv.at<cv::Vec3b>(p.y(), p.x());
    // The pixel coordinate is logged because this doubles as a measuring tool:
    //   f_x  - click both edges of a known-width object at a known distance,
    //          f_x = pixel_span * distance / real_width
    //   roll - click a plumb line near the top of the frame and near the bottom;
    //          equal x means the camera is not twisted in its mount.
    // camId is here because both workers log into the same console.
    qDebug() << "[sample] cam" << _camId << "at px" << p.x() << p.y()
             << "-> H" << px[0] << "S" << px[1] << "V" << px[2];
    cv::Mat mask = cv::Mat::zeros(hsv.size(), CV_8U);
    cv::circle(mask, cv::Point(p.x(), p.y()), 10, 255, cv::FILLED);
    samplePixels(hsv, mask);
    qDebug() << "[sample] pool size:" << _poolH.size();
    computeBounds();
}

void VisionWorker::resetSamples() {
    _poolH.clear();
    _poolS.clear();
    _poolV.clear();

    _lower = cv::Scalar();
    _upper = cv::Scalar();
    qDebug() << "[sample] resetting samples";
    _settings.remove("calib");
}

void VisionWorker::setAutoCalibrate(bool on) {
        _autoCalib = on;
        _stableCount = 0;
        qDebug() << "[calib] auto calibration" << on;
}

void VisionWorker::computeBounds() {
    if (_poolH.size() < 50) return;

    std::vector<uchar> h = _poolH, s = _poolS, v = _poolV;
    std::sort(h.begin(), h.end());
    std::sort(s.begin(), s.end());
    std::sort(v.begin(), v.end());

    int medianH = h[h.size() / 2];
    // S floor at the 25th percentile, not the 10th: under warm light the background's own
    // saturation climbs to 134-167 (the lamp is coloured, so everything it lights is), and a
    // 10th-percentile floor sits low enough to let that in.
    int loS = s[s.size() / 8];
    // V stays at the 10th percentile on purpose - brightness never separated ball from
    // background in the measurements, and tightening it would reject the ball's shadowed side.
    int loV = v[v.size() / 10];

    _lower = cv::Scalar(medianH - HUE_HALF_WIDTH, loS, loV);
    _upper = cv::Scalar(medianH + HUE_HALF_WIDTH, 255, 255);
    qDebug() << "[calib] H" << medianH - HUE_HALF_WIDTH << "-" << medianH + HUE_HALF_WIDTH << " S" << loS << " V" << loV;

    static const char* channels[3] = {"h", "s", "v"};
    for (int i = 0; i < 3; ++i) {
        _settings.setValue(QString("calib/%1Low").arg(channels[i]), _lower[i]);
        _settings.setValue(QString("calib/%1High").arg(channels[i]), _upper[i]);
    }
    qDebug() << "calib files saved";

}

void VisionWorker::detectBall(const cv::Mat &frame, const cv::Rect& roi) {
    if (_upper[2] < 1.0) {
        _detected = false;
        return;
    }

    cv::Mat crop = frame(roi);
    cv::Mat mask = buildMask(crop);

    std::vector<std::vector<cv::Point>>contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    float bestArea = 0.f;
    bool found = false;
    cv::Point2f bestCenter;
    float bestRadius = 0.f;

    double bestFill = 0.0;

    for (const auto& c : contours) {
        // measure the hull (dents bridged) or the raw outline, per the flag above
        std::vector<cv::Point> hull;
        if (USE_CONVEX_HULL) {
            cv::convexHull(c, hull);
        }
        const std::vector<cv::Point>& shape = USE_CONVEX_HULL ? hull : c;

        double area = cv::contourArea(shape);
        if (area < 200 || area > 100000) {
            continue;
        }

        cv::Point2f ctr;
        float rad = 0.f;
        cv::minEnclosingCircle(shape, ctr, rad);
        double fill = (rad > 0.f) ? area / (CV_PI * rad * rad) : 0.0;   // fills its circle? round=~0.8, streak=~0.1

        if (fill < 0.5) {
            continue;
        }

        if (area > bestArea) {
            bestArea = area;
            bestFill = fill;
            // Centre from image moments, radius from the enclosing circle. The enclosing
            // circle's centre is fixed by the two most extreme pixels, so a single bump on a
            // lumpy blob shifts it; the moments centroid is area-weighted over every pixel and
            // stays put. Matters for triangulation later - centre error becomes angle error.
            cv::Moments m = cv::moments(shape);
            bestCenter = (m.m00 > 0.0)
                ? cv::Point2f(static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00))
                : ctr;
            bestRadius = rad;
            found = true;
        }
    }

    _detected = found;
    if (found) {
       _detCenter = bestCenter + cv::Point2f(roi.x, roi.y);
       _detRadius = bestRadius;
    }


    qDebug() << "[detect]" << (found ? "BALL" : "-----") << "contours" << contours.size()
             << "r" << bestRadius << "fill" << bestFill;

}

void VisionWorker::updateTracker(const cv::Mat &frame) {
    cv::Rect full(0, 0, frame.cols, frame.rows);
    cv::Rect search = full;

    if (_trackState == TrackState::Locked) {
        int half = cvRound(_trackRadius * 3.0f);
        search = cv::Rect(cvRound(_trackCenter.x) - half, cvRound(_trackCenter.y) - half, 2 * half, 2 * half);
        search &= full;
        if (search.width <= 0 || search.height <= 0) {
            search = full;
        }
    }
    _searchRoi = search;

    detectBall(frame, search);

    if (_trackState == TrackState::Searching) {
        if (_detected) {
            _trackState = TrackState::Locked;
            _trackCenter = _detCenter;
            _trackRadius = _detRadius;
            _missCount = 0;
            qDebug() << "[track] acquired at" << _trackCenter.x << _trackCenter.y;
        }
    } else {
        if (_detected) {
            float a = 0.6f;
            _trackCenter = a * _detCenter + (1.0f - a) * _trackCenter;
            _trackRadius = a * _detRadius + (1.0f - a) * _trackRadius;
            _missCount = 0;
        } else {
            ++_missCount;
            if (_missCount > MISS_LIMIT) {
                _trackState = TrackState::Searching;
                qDebug() << "[track] lost after" << _missCount << "misses";
            }
        }
    }
}


