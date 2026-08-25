//
// Created by Andrei on 17.08.2026.
//
#include "Triangulator.h"
#include <cmath>
#include <cstdio>

using namespace tri_detail;

// scafolding

static int g_failures = 0;
static int g_checks = 0;      // increment at the top of check() and checkTrue()





static void check(const char* name, double got, double want, double tol) {
    ++g_checks;
    double err = std::fabs(got - want);
    if (err > tol) {
        std::printf("FAIL %-32s got %12.4f want %12.4f (err %.4g)\n",
            name, got, want, err);
        ++g_failures;
    } else {
        std::printf("ok    %-32s %12.4f\n", name, got);
    }
}

static void checkTrue(const char* name, bool got) {
    ++g_checks;
    if (!got) {
        std::printf("FAIL  %-32s expected true\n", name);
        ++g_failures;
    } else {
        std::printf("ok     %-32s\n", name);
    }
}
// ─── a rig to test against ───────────────────────────────────────────────────
static RigConfig makeTestRig() {
    RigConfig c = {};
    c.baseline    = 500.0;
    c.dLat        = -70.31;   // NEGATIVE: camera sits to the head's LEFT (headRight(0) = +X)
    c.dFwd        = 15.0;
    c.fx          = 615.0;
    c.fy          = 615.0;
    c.frameWidth  = 640.0;
    c.frameHeight = 480.0;
    c.stepsPerDeg = 3200.0 / 360.0;
    c.zeroSteps[0] = 0;   c.zeroSteps[1] = 0;
    c.panSign[0]   = 1.0; c.panSign[1]   = 1.0;
    // RigConfig c = {} leaves these at ZERO, and a tiltSign of 0 silently multiplies
    // every tilt angle to nothing. Set them explicitly.
    c.zeroTiltSteps[0] = 0;   c.zeroTiltSteps[1] = 0;
    c.tiltSign[0]      = 1.0; c.tiltSign[1]      = 1.0;
    c.camHeight        = 300.0;
    c.minVergenceSin = 0.01;
    return c;
}
// ─── the levels ──────────────────────────────────────────────────────────────
static void level1_helpers() {
    RigConfig cfg = makeTestRig();

    check("headForward(0).x",   headForward(0.0).x,      0.0, 1e-12);
    check("headForward(0).y",   headForward(0.0).y,      1.0, 1e-12);
    check("headForward(90).x",  headForward(PI/2).x,     1.0, 1e-12);
    check("headRight(0).x",     headRight(0.0).x,        1.0, 1e-12);
    check("headRight(90).y",    headRight(PI/2).y,      -1.0, 1e-12);

    check("panAxis(0).x",       panAxis(0, cfg).x,    -250.0, 1e-12);
    check("panAxis(1).x",       panAxis(1, cfg).x,     250.0, 1e-12);

    check("inFrameX(0)",        inFrameX(0.0, cfg),      0.0, 1e-12);

    Bearing b = {};
    b.panSteps = 800;                       // 800 * 0.1125 deg = 90 deg
    check("panAngle(800 steps)", panAngle(b, 0, cfg), PI/2, 1e-9);

    // head pointing straight ahead: camera sits dLat to the side, dFwd forward
    check("cameraXY(0,psi=0).x", cameraXY(0, 0.0, cfg).x, -250.0 - 70.31, 1e-9);
    check("cameraXY(0,psi=0).y", cameraXY(0, 0.0, cfg).y,   15.0,         1e-9);
    // psi = 90 deg: r = (0,-1), u = (1,0) -> offsets swap axes
    check("cameraXY(0,psi=90).x", cameraXY(0, PI/2, cfg).x, -250.0 + 15.0,  1e-9);
    check("cameraXY(0,psi=90).y", cameraXY(0, PI/2, cfg).y,        +70.31,  1e-9);
}
static void level2_handComputed() {
    RigConfig cfg = makeTestRig();
    cfg.dLat = 0.0;                 // cameras on the pan axes, pure geometry
    cfg.dFwd = 0.0;

    Bearing g0 = {};  g0.panSteps =  125;    // psi0 = +14.0625 deg
    Bearing g1 = {};  g1.panSteps = -125;    // psi1 = -14.0625 deg

    Fix f = triangulate(g0, g1, cfg);

    // Independent hand check: head 0 at (-250,0) aiming inward by 14.0625 deg
    // hits the centreline where 250/y = tan(14.0625 deg) = 0.2504865
    //   -> y = 250 / 0.2504865 = 998.058
    checkTrue("level2 valid", f.valid);
    check("level2 x",        f.x,        0.0,     1e-6);
    check("level2 y",        f.y,      998.058,   0.01);
    check("level2 distance", f.distance, 998.058, 0.01);
}
// Steps that point gimbal g approximately at (x,y). Deliberately ignores dLat/dFwd,
// so the aim lands slightly off, errX comes out small but NON-ZERO, and the pixel
// path gets exercised for free without shoving the ball out of frame.
static long roughAim(double x, double y, int g, const RigConfig& cfg) {
    Vec2 p = panAxis(g, cfg);
    double psiDeg = std::atan2(x - p.x, y - p.y) * 180.0 / PI;
    return cfg.zeroSteps[g] + std::lround(cfg.panSign[g] * psiDeg * cfg.stepsPerDeg);
}

// invent a target -> ask both gimbals what they WOULD report -> feed that back in
// -> demand the target back.
static void roundTrip(const char* name, double x, double y, double z,
                      long off0, long off1, const RigConfig& cfg) {
    Bearing b0 = project(x, y, z, 0, roughAim(x, y, 0, cfg) + off0, 0, cfg);
    Bearing b1 = project(x, y, z, 1, roughAim(x, y, 1, cfg) + off1, 0, cfg);

    // Guards on the TEST, not on the solver: if these trip, the chosen aim has put
    // the ball outside the frame and the case is physically impossible.
    checkTrue("  errX0 in frame", std::fabs(b0.errX) < 1.0);
    checkTrue("  errX1 in frame", std::fabs(b1.errX) < 1.0);
    checkTrue("  errY0 in frame", std::fabs(b0.errY) < 1.0);
    checkTrue("  errY1 in frame", std::fabs(b1.errY) < 1.0);

    Fix f = triangulate(b0, b1, cfg);
    checkTrue(name, f.valid);
    check(name, f.x, x, 1e-6);
    check(name, f.y, y, 1e-6);
    check(name, f.z, z, 1e-6);
    // distance is HORIZONTAL, slant is 3D. Mixing them up while validating against a
    // tape measure is worth ~45 mm at 1 m, which would read as a phantom bias.
    check("  distance", f.distance, std::hypot(x, y),                 1e-6);
    check("  slant",    f.slant,    std::sqrt(x*x + y*y + z*z),       1e-6);
}

static void level3_roundTrip() {
    RigConfig cfg = makeTestRig();
    roundTrip("rt centreline",     0.0, 1000.0, 300.0,   0,   0, cfg);
    roundTrip("rt asymmetric",   300.0, 1500.0, 500.0,   0,   0, cfg);  // the important one
    roundTrip("rt other side",  -400.0,  800.0, 150.0,   0,   0, cfg);
    roundTrip("rt close",        250.0,  600.0, 300.0,  20, -20, cfg);  // forced off-centre
    roundTrip("rt far",          100.0, 2500.0, 400.0, -15,  15, cfg);
}

static void level4_calibration() {
    RigConfig cfg = makeTestRig();
    // Nothing is ever zero on real hardware, and the two signs need not match.
    cfg.zeroSteps[0] = 1234;  cfg.zeroSteps[1] = -567;
    cfg.panSign[0]   = -1.0;  cfg.panSign[1]   =  1.0;

    roundTrip("cal centreline",   0.0, 1000.0, 300.0,  0,   0, cfg);
    roundTrip("cal asymmetric", 300.0, 1500.0, 500.0, 10, -10, cfg);
}

static void level5_guards() {
    RigConfig cfg = makeTestRig();

    // Both heads pointing the same way: rays are parallel, no intersection exists.
    Bearing p0 = {};
    Bearing p1 = {};
    Fix par = triangulate(p0, p1, cfg);
    checkTrue("parallel rejected", !par.valid);
    checkTrue("parallel not NaN",  !std::isnan(par.x) && !std::isnan(par.y));

    // Heads splayed outward: rays diverge going forward, so they "meet" BEHIND the
    // rig. Geometry says yes, physics says no.
    Bearing d0 = {}; d0.panSteps = -500;
    Bearing d1 = {}; d1.panSteps =  500;
    Fix beh = triangulate(d0, d1, cfg);
    checkTrue("behind-rig rejected", !beh.valid);
}

int main() {
    level1_helpers();
    level2_handComputed();
    level3_roundTrip();
    level4_calibration();
    level5_guards();

    std::printf("\n%s - %d check(s), %d failure(s)\n",
            (g_failures || g_checks == 0) ? "FAILED" : "PASSED", g_checks, g_failures);
    // An empty suite must NOT exit 0 - otherwise a script reads success from a run
    // where nothing was checked.
    return (g_failures || g_checks == 0) ? 1 : 0;
}