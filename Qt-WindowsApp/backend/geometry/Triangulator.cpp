//
// Created by Andrei on 17.08.2026.
//

#include "Triangulator.h"
#include <cmath>

namespace tri_detail{


    double deg2rad(double d) {
        return d * PI / 180;
    }

    // ── steps -> angles ──────────────────────────────────────────────────────────
    // Both take the raw counter and produce radians in the world frame. panSign is a
    // MEASURED +-1: whether a rising step count swings the head toward +X or away.


    double panAngle(const Bearing& b, int gimbal, const RigConfig& cfg) {
        double deg = (b.panSteps - cfg.zeroSteps[gimbal]) / cfg.stepsPerDeg;
        return deg2rad(cfg.panSign[gimbal] * deg);
    }

    double tiltAngle(const Bearing& b, int gimbal, const RigConfig& cfg) {
        double deg = (b.tiltSteps - cfg.zeroTiltSteps[gimbal]) / cfg.stepsPerDeg;
        return deg2rad(cfg.tiltSign[gimbal] * deg);
    }

    // ── pixels -> angles ─────────────────────────────────────────────────────────
    // errX/errY arrive normalised to -1..+1 by VisionWorker, so multiply back up by
    // the half-dimension before dividing by focal length.

    // The minus was MISSING until 2026-08-19, and it cost an entire day of calibration
    // that could not converge. VisionWorker runs cv::flip with FLIP_CODE = 1 before
    // anything measures errX, so in that mirrored frame a positive errX puts the ball to
    // the head's LEFT, which is NEGATIVE rotation. Same reason inFrameY carries one.
    //
    // PROVEN from the position stream, and this is the test to repeat if it is ever
    // doubted again. Hold the ball still, let one head settle, nudge ONLY that head:
    //
    //   steps 655/859   errX cam0 -0.0047   cam1 +0.0396
    //   steps 655/863   errX cam0 -0.0049   cam1 +0.0194
    //
    // Cam 0's head did not move and its errX did not move - that is the control, and it
    // says the change on cam 1 is rotation and not noise. Gimbal 1 swung -0.45 deg. The
    // ball never moved, so th1 = psi1 + inFrameX MUST come out unchanged, which needs
    // inFrameX to rise by +0.45 deg. errX FELL. Only a negated form does that.
    //
    // Why it hid for so long: with the ball centred this term is a fraction of a degree
    // and enters both bearings alike, so it nearly cancels out of the difference. It
    // stops cancelling the moment the two heads sit at different offsets - which is
    // always, because DEADZONE lets each head stop up to 1.5 deg out independently.
    // Wrong-signed, it turns that 1.5 deg of slack into 3 deg of error that changes with
    // every ball position. That is not noise, but it scatters exactly like noise, and it
    // is what defeated three separate attempts to fit zeroSteps.
    double inFrameX(double errX, const RigConfig& cfg) {
        return -std::atan(errX * (cfg.frameWidth / 2.0) / cfg.fx);
    }

    double inFrameY(double errY, const RigConfig& cfg) {
        // Image rows count DOWNWARD but elevation counts UP, so a positive errY (ball
        // BELOW the aim point) contributes NEGATIVE elevation. Hence the minus.
        //
        // NOTE: a round-trip test CANNOT verify this. project() shares the same
        // convention, so a flipped sign cancels out and the suite still passes. The
        // only proof is a real target measurably above or below the cameras.
        return -std::atan(errY * (cfg.frameHeight / 2.0) / cfg.fy);
    }


    Vec2 headForward(double psi) {
        return { std::sin(psi), std::cos(psi)};
    }

    Vec2 headRight(double psi) {
        return {std::cos(psi), -std::sin(psi)};
    }

    // ── where things are ─────────────────────────────────────────────────────────

    Vec2 panAxis(int gimbal, const RigConfig& cfg) {
        double half = cfg.baseline / 2.0;
        return {(gimbal == 0) ? -half: half, 0.0};
    }

    // The camera is NOT at the pan axis - it swings on a dLat-radius circle as the head
    // turns. Note this takes psi, not theta: the ball's position within the frame does
    // not move the camera body.
    Vec2 cameraXY(int gimbal, double psi, const RigConfig& cfg) {
        Vec2 p = panAxis(gimbal, cfg);
        Vec2 r = headRight(psi);
        Vec2 u = headForward(psi);
        return { p.x + cfg.dLat * r.x + cfg.dFwd * u.x,
                    p.y + cfg.dLat * r.y + cfg.dFwd * u.y};
    }
}

using namespace tri_detail;

Fix triangulate(const Bearing &g0, const Bearing &g1, const RigConfig &cfg) {
    Fix fix = {};
    fix.valid = false;

    // ── 1. bearings ─────────────────────────────────────────────────────────
    double psi0 = panAngle(g0, 0, cfg);
    double psi1 = panAngle(g1, 1, cfg);
    double th0  = psi0 + inFrameX(g0.errX, cfg);
    double th1  = psi1 + inFrameX(g1.errX, cfg);

    // ── 2. ray origins ─────────────────────────────────────────────────────────

    Vec2 c0 = cameraXY(0, psi0, cfg);
    Vec2 c1 = cameraXY(1, psi1, cfg);

    // ── 3. vergence guard ─────────────────────────────────────────────────────────
    // det works out to sin(th1 - th0): the sine of the convergence angle. Parallel
    // rays mean the target is effectively at infinity and there is no intersection
    // to find - so this guard IS the max-range check, falling out of the algebra.
    fix.vergence = th1 - th0;
    double det = std::sin(fix.vergence);
    if (std::fabs(det) < cfg.minVergenceSin) {
        return fix;
    }

    // ── 4. solve the 2x2 ───────────────────────────────────────────────
    Vec2 u0 = headForward(th0);
    Vec2 u1 = headForward(th1);
    Vec2 d = { c1.x - c0.x, c1.y - c0.y};

    // det already computed above as sin(th1 - th0), and already guarded
    double t0 = (d.x * (-u1.y) - (-u1.x) * d.y) / det;

    if (t0 <= 0.0) {
        return fix;
    }

    double x = c0.x + t0 * u0.x;
    double y = c0.y + t0 * u0.y;

    // ── 5. elevation, once per gimbal ────────────────────────────────────────
    // Azimuth was exactly determined - two rays, two unknowns. Elevation is OVER-
    // determined: each head reports the same height independently. Keep both and
    // publish the disagreement; it is the only thing in this file that can detect
    // a mis-set tilt zero, a head off level, or a twisted beam.

    double d0 = std::hypot(x - c0.x, y - c0.y);
    double d1 = std::hypot(x - c1.x, y - c1.y);
    double z0 = cfg.camHeight + d0 * std::tan(tiltAngle(g0, 0, cfg) + inFrameY(g0.errY, cfg));
    double z1 = cfg.camHeight + d1 * std::tan(tiltAngle(g1, 1, cfg) + inFrameY(g1.errY, cfg));

    // ── 6. result ────────────────────────────────────────────────────────────
    fix.x = x;
    fix.y = y;
    fix.z = 0.5 * (z0 + z1);
    fix.zDisagreement = std::fabs(z0 - z1);
    fix.distance = std::hypot(x, y);
    fix.slant    = std::sqrt(x * x + y * y + fix.z * fix.z);
    fix.valid = true;
    return fix;
}
// Forward model. Runs the pipeline backwards so the round-trip test can exist:
// invent a target, ask what each gimbal WOULD report, feed that back to
// triangulate(), and check you get your target back.
Bearing project(double x, double y, double z, int gimbal,
                long aimPanSteps, long aimTiltSteps, const RigConfig &cfg) {
    Bearing b = {};
    b.panSteps  = aimPanSteps;
    b.tiltSteps = aimTiltSteps;

    // where the head is aimed -> where the camera body actually sits
    double psi = panAngle(b, gimbal, cfg);
    Vec2   c   = cameraXY(gimbal, psi, cfg);

    // Horizontal: true bearing to the target, minus where the head is aimed.
    // atan2 takes dx FIRST here - angles in this file run from +Y, not from +X.
    double theta = std::atan2(x - c.x, y - c.y);
    double phi   = theta - psi;
    b.errX = std::tan(phi) * cfg.fx / (cfg.frameWidth / 2.0);

    // Vertical: same story, but elevation is measured off the HORIZONTAL distance.
    // The minus mirrors the one in inFrameY - these two must stay exact inverses.
    double beta  = tiltAngle(b, gimbal, cfg);
    double horiz = std::hypot(x - c.x, y - c.y);
    double eps   = std::atan2(z - cfg.camHeight, horiz);
    b.errY = -std::tan(eps - beta) * cfg.fy / (cfg.frameHeight / 2.0);

    return b;
}



