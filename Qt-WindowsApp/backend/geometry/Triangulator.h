//
// Created by Andrei on 17.08.2026.
//

#ifndef QT_WINDOWSAPP_TRIANGULATOR_H
#define QT_WINDOWSAPP_TRIANGULATOR_H

// Pure geometry. No Qt, no OpenCV, no hardware — so it can be tested with invented
// numbers long before the beam exists.
//
// UNITS: millimetres, and RADIANS internally. Degrees and steps only at the boundary.
//
// WORLD FRAME: origin at the midpoint between the two pan axes.
//   +X along the baseline toward gimbal 1
//   +Y forward, away from the rig
//   angles measured from +Y, positive rotating toward +X
// Gimbal 0 pan axis sits at (-B/2, 0), gimbal 1 at (+B/2, 0).

// Triangulator.h — add above the public API



struct RigConfig {
    double baseline;            //B, pan axis to pan axis.
    double dLat;                // camera offsewt from pan axis
    double dFwd;                // optical center forward of the pan axis
    double fx;                  // focal lengths in pixels
    double fy;
    double frameWidth;          // 640
    double frameHeight;
    double stepsPerDeg;         // 8.889
    long zeroSteps[2];          // steps frrom home to "pointing straight ahead", per gimbal
    long zeroTiltSteps[2];
    double panSign[2];          // +1 if rising step count pans toward +X
    double tiltSign[2];
    double minVergenceSin;
    double camHeight;
};

struct Bearing {
    long panSteps;
    long tiltSteps;
    double errX;                // -1..+1
    double errY;
};

struct Fix {
    double x, y, z;
    double distance;            // Apollonius' AD: HORIZONTAL range from the origin
    double slant;               // 3D range from the origin - what a tape measure from
                                // the centre hole actually reads. Validate against this.
    double vergence;            // theta1 - theta0, radians. diagnostics + range gate.
    double zDisagreement;
    bool valid;
};

Fix triangulate(const Bearing& g0, const Bearing& g1, const RigConfig& cfg);

Bearing project(double x, double y, double z, int gimbal,
                long aimPanSteps, long aimTiltSteps, const RigConfig& cfg);

namespace tri_detail {
    struct Vec2 { double x, y; };

    double deg2rad(double d);
    double panAngle(const Bearing& b, int gimbal, const RigConfig& cfg);
    double tiltAngle(const Bearing& b, int gimbal, const RigConfig& cfg);
    double inFrameX(double errX, const RigConfig& cfg);
    double inFrameY(double errY, const RigConfig& cfg);
    Vec2   headForward(double psi);
    Vec2   headRight(double psi);
    Vec2   panAxis(int gimbal, const RigConfig& cfg);
    Vec2   cameraXY(int gimbal, double psi, const RigConfig& cfg);
    constexpr double PI = 3.14159265358979323846;
}

#endif //QT_WINDOWSAPP_TRIANGULATOR_H
