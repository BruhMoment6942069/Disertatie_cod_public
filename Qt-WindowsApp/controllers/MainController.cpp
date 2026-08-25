//
// Created by Andrei on 2/4/2026.
//

#include "MainController.h"
#include <QPushButton>
#include "../backend/services/AppService.h"
#include "../frontend/MainWindow.h"
#include <QDebug>
#include  <QThread>
#include <QDateTime>
#include <cmath>
#include <algorithm>
#include <cstdlib>

// These three are a set - changing one alone usually undoes what you wanted.
//
//   DEADZONE  how close is "close enough", normalised so 1.0 is half a frame. It is a
//             POINTING tolerance, no longer an accuracy one: with inFrameX finally
//             signed correctly the solver reads the leftover offset out of the image
//             and adds it to the bearing, so a ball resting off-centre costs nothing.
//             Tightened 0.05 -> 0.02 for the demo, not for the measurement: 0.05 let the
//             ball sit 16px out, which looks like the rig is ignoring it.
//   GAIN      deg/s per unit of error. Sets how hard it converges once close.
//   MAX_DEG_S clamp. Sets how fast it crosses a LARGE gap, because anything past
//             MAX_DEG_S/GAIN of error saturates and gain stops mattering there.
//
// Raising GAIN alone will not speed up acquisition - that is MAX_DEG_S. Raising
// MAX_DEG_S alone will not tighten settling - that is GAIN and DEADZONE.
//
// Why tightening DEADZONE is safe: the head travels GAIN*DEADZONE*T per frame at the
// smallest non-zero command, and the deadzone is worth about 22*DEADZONE degrees, so the
// no-overshoot condition is GAIN*T < 22 - DEADZONE cancels out entirely. At 15fps that
// caps GAIN near 330. 100 leaves a wide margin for stepper acceleration and UDP latency,
// neither of which that estimate accounts for.
//
// If a head starts buzzing around the crosshair instead of stopping on it, that is a
// limit cycle: drop GAIN first, not DEADZONE. Hunting is a gain problem.
static constexpr double DEADZONE = 0.02;
static constexpr double GAIN = 100.0;
static constexpr double MAX_DEG_S = 30.0;

// Applied once per frame while the tracker is coasting through a detection dropout.
// Integer halving truncates toward zero, so it reaches an exact 0 unaided:
// 20 -> 10 -> 5 -> 2 -> 1 -> 0. Five frames, comfortably inside VisionWorker's
// MISS_LIMIT of ten, so the axis is already stopped well before lock is dropped.
// A named function rather than an inline expression because both axes share it and
// the decay rate is a tuning knob worth finding in one place.
static qint32 decayCommand(qint32 v) {
    return v / 2;
}

// Loop polarity, one entry per axis. +1 means "a positive command moves the camera TOWARD a
// ball sitting on the positive-error side of the frame"; -1 means that axis is the other way.
//
// This is a MEASURED value, not a derived one. It is the product of several independent +-1
// links nobody can read off a schematic: motor coil pair order, DIR level, gearing, how the
// head is bolted together, and the horizontal flip in VisionWorker. Get it wrong and the loop
// is positive feedback - the error grows, the command saturates at MAX_DEG_S, and the axis
// runs until its soft limit catches it.
//
// Measure it per axis: hold the ball still off-centre, drag that axis' test slider until the
// ball reaches frame centre. Slider went up -> +1. Slider went down -> -1.
// Re-measure after any rebuild of the head, and after changing FLIP_CODE (horizontally
// affects the pan entries, vertically the tilt entries).
//
// Index matches _speeds: 0 = G1 pan, 1 = G1 tilt, 2 = G2 pan, 3 = G2 tilt.
//
// ALL FOUR MEASURED +1, confirmed by end-to-end closed-loop tracking on 2026-08-12.
// History worth keeping: before that date pan and tilt were wired to each other's
// drivers, so each axis chased the other's error and no amount of sign-flipping here
// could fix it - a swap is immune to a sign change. The cure was moving the cables, not
// this array. If tracking ever runs away again, check for a swap BEFORE editing these.
//
// Triangulation leans on this too: +1 on a pan axis means errX and a rising step count
// point the same way, which is exactly what Triangulator's inFrameX assumes when it adds
// phi to psi without a negation. If a panSign in RigConfig comes out disagreeing with the
// matching entry here, one of the two is wrong - they are not independent measurements.
static constexpr double AXIS_SIGN[4] = {1.0, 1.0, 1.0, 1.0};

// The two cameras run free of each other, so their frames are never simultaneous.
// Beyond this much skew the pair describes two different moments and the "fix" is
// a fiction - at 20 deg/s a head moves 0.4 deg in 20ms, worth ~70mm at 2m.
static constexpr qint64 MAX_PAIR_SKEW_MS = 40;
// Losing lock is reported inline. A camera that dies outright reports NOTHING, so
// only a timer can notice the readout has gone stale.
static constexpr int  FIX_WATCHDOG_MS  = 250;
static constexpr qint64 FIX_STALE_MS   = 400;
// The bearings had freshness checks from the start; the motor positions did not, and
// on 2026-08-18 that cost a whole evening. The MOTOR_POS stream stopped while vision
// kept running, so the heads went on tracking perfectly - the speed loop reads errX
// and never touches _panSteps - while every fix was solved from step counts 120 steps
// out of date. Output stayed stable, repeatable, and dZ 4mm. It read 1033mm at every
// distance because its dominant input had stopped being a variable.
//
// A dead sensor MUST make the readout go blank, never confident. Positions arrive at
// ~50Hz, so anything past 150ms means several missed packets, not jitter.
static constexpr qint64 POS_STALE_MS   = 150;

// EVERY NUMBER BELOW IS A PLACEHOLDER until it is measured on the real rig.
// Nothing here is a guess you can leave in: baseline scales every distance you will
// ever report, and a wrong sign puts the ball on the wrong side of the world.
//
//   baseline   MEASURE boss-to-boss, or better: FIT it from one accurately known
//              long distance, since 2000mm to +-2mm beats 500mm to +-1mm as a fraction
//   dLat       -70.31 from CAD. NEGATIVE because the camera sits to the head's LEFT
//              and headRight(0) points +X. Same on both heads - they are not mirrored.
//   dFwd       optical centre ahead of the pan axis. Smallest term, fit it last.
//   fx / fy    MEASURE per camera: known width at known distance, count pixels.
//   zeroSteps  MEASURE per gimbal, OPTICALLY - the step count at which that camera
//              sees straight ahead. Absorbs motor zero, backoff overshoot, mount
//              slop, camera yaw and optical-centre offset all in one number.
//   panSign    MEASURE per gimbal: does a rising step count swing the head toward +X?
static RigConfig makeRig() {
    RigConfig c = {};
    // 494.5, not the CAD 500. FITTED, and only trustworthy because the zeros below were
    // fitted from the same two points in the same pass - solve them separately and each
    // absorbs the other's error.
    //
    // CAD is right about offsets and wrong about scale factors. dLat and camHeight are
    // small enough that print tolerance is sub-millimetre on them. Baseline is 500mm of
    // printed plate, so 1.1% of it is 5.5mm, and it multiplies EVERY distance the rig
    // will ever report. It is the one dimension here that had to be measured rather
    // than modelled.
    c.baseline    = 496.5;
    c.dLat        = -70.31;
    c.dFwd        = 13.0;       // CAD, upper bound. Smallest term in the rig: 10mm of
                                // error here is worth ~2mm in the answer, so "max 13"
                                // is already past the point of diminishing returns.
    // 820, not the 615 placeholder. MEASURED from the position stream without any test
    // chart: park the ball, nudge ONE head a known number of steps, and read how much
    // errX moved on that camera. 4 steps is 0.45deg and moved errX by 0.0202, so
    //     0.0202 * (frameWidth/2) / fx = tan(0.45deg)  ->  fx = 823
    // The ball-radius route agreed independently at ~780 (r = fx * 32mm / distance).
    //
    // Only frameWidth/fx as a RATIO matters, never either alone - if the camera does not
    // actually deliver 640 wide, the error is absorbed here, because errX arrives already
    // normalised to +-1 by VisionWorker. Do not "fix" frameWidth without refitting this.
    c.fx          = 820.0;
    c.fy          = 820.0;
    c.frameWidth  = 640.0;
    c.frameHeight = 480.0;
    c.stepsPerDeg = 3200.0 / 360.0;      // direct drive, verified: no reduction

    // MEASURED 2026-08-18, straight off the position stream: every run opens with
    // positions (694, 240, 694, 240) and holds. The same count on BOTH heads, run
    // after run, is what makes these usable as zeros at all - and it is the payoff
    // for H_PARK waiting on getSpeed() instead of exiting on position alone. Before
    // that fix pan settled wherever its coast happened to end (755 one run).
    //
    // CORRECTED 2026-08-18 from a tape-measured ball. Park is still (694, 240, 694,
    // 240) on both heads - do NOT read these as "where the heads park". They are
    // where the heads park PLUS a per-head offset, because parking at the same step
    // count does not mean the two heads point the same way: each homes against its
    // own endstop, and the two switches trip a few degrees apart on printed brackets.
    //
    // FITTED 2026-08-19 from FOUR tape-measured positions, and only after the inFrameX
    // sign in Triangulator.cpp was corrected. Every fit attempted before that failed at
    // the next distance, because a wrong-signed inFrameX doubled each head's DEADZONE
    // residual instead of removing it - up to 3 deg of vergence error that changed with
    // every ball position and scattered exactly like noise.
    //
    //   tape 1700  read 1822  +7.2%   verg 15.4deg
    //   tape 1170  read 1219  +4.2%   verg 22.9deg
    //   tape  870  read  894  +2.8%   verg 30.9deg
    //   tape  460  read  472  +2.6%   verg 55.0deg
    //
    // Monotonic in vergence, which is the differential-zero signature and nothing else.
    // Regressing error against 1/vergence over all four gives delta = 1.03deg with a
    // baseline term of 0.1% - so it is one differential offset, 9 steps, split +-4.5 so
    // the bisector does not move. Residuals after: -0.2, -0.3, -0.6, +0.6 percent.
    //
    // READ THE SHAPE, NOT THE SIZE. Growing percentage as range grows = zeros. Flat
    // percentage = baseline. Percentage that jumps around = something upstream is wrong
    // and fitting will only chase it. That last case is what cost the whole day.
    //
    // FOUR points, TWO unknowns. Three separate two-point fits were made earlier and all
    // three were wrong, because two points can always be satisfied exactly and so can
    // never be checked. Over-determination is not rigour for its own sake here; it is the
    // only thing that distinguishes a fit from a coincidence.
    //
    // The +112 on BOTH is common-mode, pinned separately and by a different measurement.
    // Distance is blind to it - rotating both bearings together swings the answer
    // sideways while the vergence between them, and so the range, is untouched. It took
    // a LATERAL observation to see it at all: with the ball on the bisector the app
    // still placed it 219mm to one side, 12.6deg out.
    //
    // Why common-mode and differential have different sizes and different causes:
    //
    //   common-mode  12.6deg  BOTH heads use M*_PARK_DEG = 75.0, so an error in that
    //                         one number lands identically on both. Shared constant,
    //                         shared error. Tuned by eye, and 75 is not straight ahead.
    //   differential  2.85deg Each head homes against its OWN endstop, and two switches
    //                         on printed brackets do not trip at the same angle.
    //
    // That is why one number could never fix this: they are two faults with two causes,
    // visible to two different measurements. Range sees only the difference, lateral
    // position sees only the sum.
    //
    // x is the least-verified number here - it rests on "the tape reads about the same
    // from each pan axis", not on a real bisector construction. slant does not depend on
    // it at all: re-solving with and without the +112 moved distance by 0.6%.
    c.zeroSteps[0]     = 813;  c.zeroSteps[1]     = 799;
    c.zeroTiltSteps[0] = 215;  c.zeroTiltSteps[1] = 248;

    // MEASURED from the settled state of that same run: both heads locked on one
    // stationary ball with |errX| < 0.03, at 497 and 800 steps - 197 BELOW park on
    // gimbal 0, 106 ABOVE it on gimbal 1. Opposite step directions.
    //
    // Two identical, unmirrored heads aiming at one target a metre out must verge
    // INWARD. panAxis puts gimbal 0 at -B/2 and gimbal 1 at +B/2, so inward means
    // psi0 > 0 and psi1 < 0. Both step counts run against their psi: -1 each.
    //
    // This is NOT what AXIS_SIGN would predict, and the note above AXIS_SIGN warned
    // the two are coupled. They are not, quite: AXIS_SIGN is a loop-polarity fact
    // about pixels vs. steps, panSign is a geometric fact about steps vs. world.
    // The horizontal mirror in VisionWorker sits between them and inverts one link.
    // See the inFrameX note in Triangulator.cpp - that is where the mirror belongs.
    c.panSign[0]       = -1.0; c.panSign[1]       = -1.0;
    // tiltSign was +1 until 2026-08-19 and it was simply backwards. PROVEN from the
    // control loop, needing no tape and no geometry:
    //
    //   [track] err -0.198158 , 0.299562 -> deg/s -12 , 18
    //   positions: QList(693, 241, ...)   tilt climbing 240 -> 241 -> 242
    //
    // Image rows count DOWNWARD, so errY > 0 means the ball is BELOW the aim point.
    // The loop answered with a positive tilt speed and the step count ROSE. Therefore
    // rising tilt steps aim the camera DOWN. Elevation counts UP, so the conversion
    // from steps to elevation must carry a minus.
    //
    // The corroboration: with +1 the tilt zeros had to sit 96 and 157 steps off park
    // to explain a ball at plate height. With -1 they land 25 and 8 steps off park,
    // which is what "the park position looks roughly level" is supposed to mean. A
    // sign error hides as an enormous offset in the constant that follows it.
    c.tiltSign[0]      = -1.0; c.tiltSign[1]      = -1.0;
    // MEASURE: lens optical centre up to the PLATE surface - the plane the dot sits
    // in. NOT to the floor. This is the datum for z, and slant is sqrt(x^2+y^2+z^2)
    // with x and y taken from the pan-axis midpoint, which is the dot. Measuring
    // camHeight to the plate is precisely what makes slant the number a tape stretched
    // from the dot to the ball reads. Measure it to the floor and slant silently mixes
    // two datums and over-reads.
    //
    // Touches z and slant ONLY: x, y and distance come from the pan bearings alone and
    // never see this number. Getting it wrong is a close-range bias that shrinks with
    // distance - a 180mm mistake costs ~37mm at 1m but only ~13mm at 3m - which is why
    // it has to be right BEFORE fitting the baseline. The near points are what pin the
    // slope down, and those are the ones this corrupts.
    // From CAD, at the parked tilt angle. CAD is the RIGHT source for this one and the
    // wrong one for baseline, which looks inconsistent until you notice why: print
    // shrinkage is fractional, and 0.5% of 124mm is sub-millimetre while 0.5% of 500mm
    // is 2.5mm of pure scale error on every distance the rig will ever report. Offsets
    // tolerate a fractional error; scale factors do not.
    c.camHeight      = 124.41;
    // sin(vergence) below this means the rays are effectively parallel: the target is
    // too far to triangulate. This IS the max-range check, straight out of the algebra.
    c.minVergenceSin = 0.01;
    return c;
}


//Constructor of MainController -> class from maincontroller header
// it receives a pointer to main window (basically shows stuff on the screen)
// if it wasn't a pointer it would make a copy and the controller would talk to the copy instead of the actual object
// window() is a member variable
// window(<<window>>) is the constructor parameter
// witthout this you can't connect the signals and talk to the UI
// service(new AppService()) this creates the backend that's taken care by the controller and the ui has no idea of the backend
// initializer list is faster than {} and is required for refeneces/const
MainController::MainController(MainWindow *window)
    : window(window), service(new AppService()), _udpClient(new UdpClient()), _notificationPop(new NotificationPop())
{
    /*
     * When window emits actionButtonClicked, call this controller's onActionButton
     * window sends the signal &MainWindow::actionButtonClicked
     * this ( the controller) this refers to the current instance is the receiver
     * and the function to call AKA slot is &MainController::onActionButton
     * &MainWindow::actionButtonClicked this is a pointer to the member function
     */
    setupVisionWorker(0);
    setupVisionWorker(1);
    connect(window, &MainWindow::actionButtonClicked, this, &MainController::onActionButton);
    connect(window, &MainWindow::startRequested, this, &MainController::handleStart);
    connect(window,  &MainWindow::resetRequested, this, &MainController::handleReset);
    connect(window, &MainWindow::statusRequested, this, &MainController::handleStatus);
    connect(window, &MainWindow::connectionRequested, this, &MainController::handleConnection);
    connect(_notificationPop, &NotificationPop::getMessage, this, &MainController::handleNotification);
    connect(_udpClient, &UdpClient::dataReceived, this, &MainController::handleNotification);
    connect(_udpClient, &UdpClient::motorPositions, this, &MainController::handleMotorPositions);
    connect(window, &MainWindow::targetRequested, this, &MainController::handleTargetSliders);
    connect(window, &MainWindow::modeChanged, this, &MainController::handleModeSent);
    connect(window, &MainWindow::engageTrackingToggled, this, &MainController::handleEngageTracking);
    connect(window, &MainWindow::cameraSelected, this, &MainController::handleCameraSelected);
    connect(window, &MainWindow::clipSelected, this, &MainController::handleClipsSelected);


    _rig = makeRig();
    connect(&_fixWatchdog, &QTimer::timeout, this, &MainController::handleFixTimeout);
    _fixWatchdog.start(FIX_WATCHDOG_MS);

    qDebug() << "MainController thread:" << QThread::currentThread();


}

MainController::~MainController() {
    for (int i = 0; i < 2; ++i) {
        if (!_visionThread[i]) {
            continue;
        }
        _visionThread[i]->quit();
        _visionThread[i]->wait();
    }
}

void MainController::onActionButton() {
    qDebug() << "[Controller] Button clicked\n";
    service->doSomething();
}
void MainController::handleStart() {
    qDebug() << "[Controller] Start requested\n";
    service->startProcess();
}
void MainController::handleReset() {
    qDebug() << "[Controller] Reset requested\n";
    service->resetProcess();
}

void MainController::handleStatus() {
    qDebug() << "[Controller] Status requested";
    service->printStatus();
}

void MainController::handleConnection() {
    _isAlreadyConnected = false;
    service->connectionStarted();
    _udpClient->send_Message("Connected");

}

void MainController::handleMotorPositions(const QList<qint32> &motorPositions, bool homed) {
    // Keep the freshest angles for triangulation. Order matches the ESP's motors[]:
    // 0 = G1 pan, 1 = G1 tilt, 2 = G2 pan, 3 = G2 tilt.
    _panSteps[0]  = motorPositions.value(0);
    _tiltSteps[0] = motorPositions.value(1);
    _panSteps[1]  = motorPositions.value(2);
    _tiltSteps[1] = motorPositions.value(3);
    // Before homing the step counter's origin is wherever the ESP happened to boot,
    // so every angle derived from it is fiction. This flag has been arriving all
    // along and being dropped on the floor.
    _homed = homed;
    // Stamped LAST, and only on a packet that actually parsed. tryTriangulate reads
    // this to decide whether _panSteps still describes the present.
    _posMs = QDateTime::currentMSecsSinceEpoch();

    window->updateCoordinates(motorPositions);
}

void MainController::handleTargetSliders(qint32 pan, qint32 tilt) {
    _udpClient->sendTarget(pan,tilt);
}

void MainController::handleModeSent(const int mode) {
    _currentMode = mode;
    if (mode == Test_Mode) {
        _udpClient->sendMode(UdpClient::ModeID::TEST);
        qDebug() << "[Controller] Test Mode";
    }
    else if (mode == Calibration_Mode) {
        _udpClient->sendMode(UdpClient::ModeID::CALIB);
        qDebug() << "[Controller] calib Mode";
    } else if (mode == Tracking_Mode) {
        _udpClient->sendMode(UdpClient::ModeID::TRACK);
        qDebug() << "[Controller] Track Mode";
    }
}

void MainController::handleFrameReady(int id, const QImage& image) {
    // clip replay only ever runs on worker 0 and paints the clip page
    if (_playingClip && id == 0) {
        window->updateClipFrame(image);
    } else {
        window->updateVideoFrame(id, image);
    }

}

void MainController::handleCameraFailed(const QString &reason) {
    qDebug() << reason;
}

void MainController::handleFPSUpdate(int id, const double &fps) {
    window->updateFPS(id, fps);
}

void MainController::handleClipsSelected(const QString &path) {
    _playingClip = true;
    emit captureStopRequested(0);
    emit playBackStartRequested(0, path);
}

void MainController::pushSpeeds() {
    _udpClient->sendSpeed({_speeds[0], _speeds[1], _speeds[2], _speeds[3]});
}

void MainController::setupVisionWorker(int id) {
    _visionThread[id] = new QThread(this);
    _visionWorker[id] = new VisionWorker(id);
    _visionWorker[id]->moveToThread(_visionThread[id]);
    connect(_visionThread[id], &QThread::finished, _visionWorker[id], &QObject::deleteLater);
    connect(this, &MainController::captureStartRequested, _visionWorker[id], &VisionWorker::startCapture);
    connect(_visionWorker[id], &VisionWorker::frameReady, this, &MainController::handleFrameReady);
    connect(_visionWorker[id], &VisionWorker::cameraFailed, this, &MainController::handleCameraFailed);
    connect(_visionWorker[id], &VisionWorker::fpsUpdated, this, &MainController::handleFPSUpdate);
    connect(window, &MainWindow::exposureC1LockToggled, _visionWorker[id], &VisionWorker::setExposureLock);
    connect(this, &MainController::captureStopRequested, _visionWorker[id], &VisionWorker::stopCapture);
    connect(window, &MainWindow::startCalibrationRecodring, _visionWorker[id], &VisionWorker::setRecording);
    connect(this, &MainController::playBackStartRequested, _visionWorker[id], &VisionWorker::startPlayback);
    connect(window, &MainWindow::debugViewChanged, _visionWorker[id], &VisionWorker::setDebugView);
    connect(window, &MainWindow::frameSampleRequested, _visionWorker[id], &VisionWorker::sampleAt);
    connect(window, &MainWindow::calibrationResetRequested, _visionWorker[id], &VisionWorker::resetSamples);
    connect(window, &MainWindow::autoCalibrateToggled, _visionWorker[id], &VisionWorker::setAutoCalibrate);
    connect(_visionWorker[id], &VisionWorker::autoCalibrateFinished, window, &MainWindow::handleAutoCalibrateFinished);
    connect(_visionWorker[id], &VisionWorker::ballTracked, this, &MainController::handleBallTracked);
    _visionThread[id]->start();
}

void MainController::handleNotification(const QString& message) {
    qDebug() << "handleNotification called with:" << message;  // add this
    if (_isAlreadyConnected == true) {
        return;
    }
    QString cleanMessage = message.trimmed();
    qDebug() << "Raw message bytes:" << message.toUtf8().toHex();
    qDebug() << "Clean message:" << cleanMessage;
    if (cleanMessage.compare("Connected", Qt::CaseInsensitive)== 0) {
        qDebug() << "window is:" << window;
        window->showNotification(message);
        _isAlreadyConnected = true;
    } else {
        window->showNotification("Not Connected");
    }
    qDebug() << "sunt aici";
}

void MainController::handleCameraSelected(int camId, int deviceIndex) {
    _playingClip = false;
    emit captureStopRequested(camId);
    emit captureStartRequested(camId, deviceIndex);
}

void MainController::handleBallTracked(int id, bool locked, double errX, double errY) {
    // A COASTED REPEAT IS NOT A MEASUREMENT. While detection misses, updateTracker
    // holds Locked for up to MISS_LIMIT frames and re-emits the stored _trackCenter
    // untouched, so errX/errY arrive BIT-IDENTICAL. Exact comparison is correct here
    // precisely because it is the same stored value, not a recomputed one.
    //
    // BOTH paths below need to know this, for different reasons: triangulation must
    // not pair an 11-frame-old bearing against a live one and call it a fix, and the
    // speed loop must not keep commanding on an error that stopped existing.
    bool repeat = locked && _haveLastErr[id] && errX == _lastErrX[id] && errY == _lastErrY[id];

    // ── triangulation path ───────────────────────────────────────────────────
    // Deliberately NOT gated on Tracking_Mode. During calibration the heads are
    // aimed by hand with the test sliders at a target on a tape measure, and a live
    // distance readout is exactly what you need while doing it. The speed-command
    // path below keeps its own gate, because that one must only run while armed.
    if (locked) {
        if (!repeat) {
            _lastErrX[id]    = errX;
            _lastErrY[id]    = errY;
            _haveLastErr[id] = true;

            // Pair this frame with the freshest motor angles available to it.
            _bearing[id].panSteps  = _panSteps[id];
            _bearing[id].tiltSteps = _tiltSteps[id];
            _bearing[id].errX      = errX;
            _bearing[id].errY      = errY;
            _bearingFresh[id]      = true;
            _bearingMs[id]         = QDateTime::currentMSecsSinceEpoch();
            tryTriangulate();
        }
    } else {
        _bearingFresh[id] = false;
        // Forget the last error too: after a re-acquire the tracker can legitimately
        // land on the same centre again, and that IS a new measurement.
        _haveLastErr[id]  = false;
        window->updateDistance(Fix{});
    }

    // ── speed command path ───────────────────────────────────────────────────
    if (_currentMode != Tracking_Mode) {
        return;
    }

    if (!locked) {
        _repeatCount[id] = 0;
        _speeds[id * 2] = 0;
        _speeds[id * 2 + 1] = 0;
        pushSpeeds();
        qDebug() << "[track] lost -> speed 0";
        return;
    }

    if (repeat) {
        // FLYING BLIND. The tracker holds Locked for MISS_LIMIT = 10 frames of missed
        // detection, re-emitting one stale error the whole time. Re-applying it verbatim
        // commands whatever that error asked for - up to full MAX_DEG_S - for a third of
        // a second with nothing watching. On 2026-08-18 that walked gimbal 1 tilt 240
        // steps into its minus limit; only setMinusLimit(0) stopped the counter from
        // being corrupted, which would have taken the whole thesis measurement with it.
        //
        // Decay rather than a hard zero, because riding out a one- or two-frame blink is
        // the entire point of miss hysteresis - stopping dead on every flicker would make
        // tracking stutter badly at the ~50% detection rate this rig actually gets. What
        // decay buys is a bounded excursion: halving each frame, total further travel is
        // under one frame's worth of the last commanded speed.
        _repeatCount[id]++;
        _speeds[id * 2]     = decayCommand(_speeds[id * 2]);
        _speeds[id * 2 + 1] = decayCommand(_speeds[id * 2 + 1]);
        pushSpeeds();
        qDebug() << "[track] stale x" << _repeatCount[id] << "-> coasting down, deg/s"
                 << _speeds[id * 2] << "," << _speeds[id * 2 + 1];
        return;
    }
    _repeatCount[id] = 0;

    // deadzone: a near-centered axis commands zero, so centered ball rests
    double panSpeed = 0.0;
    if (std::abs(errX) >= DEADZONE) {
        panSpeed = AXIS_SIGN[id * 2] * GAIN * errX;
    }
    double tiltSpeed = 0.0;
    if (std::abs(errY) >= DEADZONE) {
        tiltSpeed = AXIS_SIGN[id * 2 + 1] * GAIN * errY;
    }

    panSpeed = std::clamp(panSpeed, -MAX_DEG_S, MAX_DEG_S);
    tiltSpeed = std::clamp(tiltSpeed, -MAX_DEG_S, MAX_DEG_S);

    qint32 pan = std::lround(panSpeed);
    qint32 tilt = std::lround(tiltSpeed);
    _speeds[id * 2] = pan;
    _speeds[id * 2 + 1] = tilt;
    pushSpeeds();
    qDebug() << "[track] err" << errX <<","<< errY << "-> deg/s" << pan << "," <<tilt;
}

void MainController::tryTriangulate() {
    // Wait for BOTH cameras to have reported since the last fix. Solving on every
    // ballTracked would double the rate but pair half of them against a bearing up
    // to a frame old, which is buying rate with accuracy - backwards for a distance
    // measurement.
    if (!_bearingFresh[0] || !_bearingFresh[1]) {
        return;
    }
    _bearingFresh[0] = false;
    _bearingFresh[1] = false;

    if (!_homed) {
        window->updateDistance(Fix{});
        return;
    }

    // Fresh pixels + dead motors still looks like a perfect fix, because the cameras
    // go on reporting and only the angles they are multiplied by have frozen. Nothing
    // downstream can tell: vergence stays plausible, both heads agree on height, the
    // number is rock steady. Steady IS the symptom. Refuse rather than publish.
    //
    // A single function-local static is safe here where it was NOT safe in the motor
    // logs - there is one position stream, not four, so no two callers alias it.
    // ONE static, edge-triggered both ways: the log must announce the recovery as
    // loudly as the failure, or a stream that dies and returns looks like it never
    // died. Two separate flags cannot do this - the recovery branch is unreachable
    // unless the same variable that suppressed the spam is the one that clears it.
    static bool posWasStale = false;
    const qint64 posAge = QDateTime::currentMSecsSinceEpoch() - _posMs;
    if (posAge > POS_STALE_MS) {
        if (!posWasStale) {
            qDebug().nospace() << "[fix] SUPPRESSED - motor positions " << posAge
                               << "ms old (limit " << POS_STALE_MS << "). The cameras are "
                               << "fine and the heads are still tracking; the MOTOR_POS "
                               << "stream is not arriving. Any fix now would be solved "
                               << "from stale step counts.";
            posWasStale = true;
        }
        window->updateDistance(Fix{});
        return;
    }
    if (posWasStale) {
        qDebug() << "[fix] motor positions live again, age" << posAge << "ms";
        posWasStale = false;
    }

    // Two bearings from two different moments do not describe one ball.
    if (std::llabs(_bearingMs[0] - _bearingMs[1]) > MAX_PAIR_SKEW_MS) {
        window->updateDistance(Fix{});
        return;
    }

    Fix fix = triangulate(_bearing[0], _bearing[1], _rig);
    window->updateDistance(fix);

    // noquote(): QString::number returns a QString and qDebug wraps those in quotes,
    // which is how the vergence came out as verg "45.38"deg in the first logs.
    double vergDeg = fix.vergence * 180.0 / tri_detail::PI;

    if (fix.valid) {
        qDebug().nospace().noquote()
            // slant FIRST because slant is the deliverable: the thesis measures a
            // radius from the dot, not a footprint on the plate. dist is kept beside
            // it as a diagnostic - it is the pure two-bearing solve, untouched by
            // camHeight or the tilt zeros, so when slant drifts and dist does not the
            // fault is vertical.
            << "[fix] slant " << qRound(fix.slant) << "mm  dist " << qRound(fix.distance)
            << "mm  xyz (" << qRound(fix.x) << "," << qRound(fix.y) << "," << qRound(fix.z)
            << ")  verg " << QString::number(vergDeg, 'f', 2)
            << "deg  dZ " << qRound(fix.zDisagreement)
            << "  steps " << _bearing[0].panSteps << "/" << _bearing[1].panSteps;
        return;
    }

    // TWO guards produce valid == false and they mean opposite things - one says the
    // target is out of range, the other says the rig is mis-calibrated. Reporting both
    // as "vergence too small" sent us hunting a range problem for a whole log that was
    // really zeroSteps being 0. Split them here rather than in the solver: fix.vergence
    // is populated before either guard trips, so nothing extra has to be plumbed out,
    // and Triangulator stays free of reporting concerns.
    if (std::fabs(std::sin(fix.vergence)) < _rig.minVergenceSin) {
        qDebug().nospace().noquote()
            << "[fix] REJECTED - rays parallel, verg " << QString::number(vergDeg, 'f', 3)
            << "deg: target is past max range. EXPECTED at distance.";
    } else {
        qDebug().nospace().noquote()
            << "[fix] REJECTED - rays cross BEHIND the rig, verg "
            << QString::number(vergDeg, 'f', 2) << "deg  steps "
            << _bearing[0].panSteps << "/" << _bearing[1].panSteps
            << ": heads are DIVERGING, so zeroSteps or panSign is wrong. Not a range issue.";
    }
}

void MainController::handleFixTimeout() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - _bearingMs[0] > FIX_STALE_MS || now - _bearingMs[1] > FIX_STALE_MS) {
        _bearingFresh[0] = false;
        _bearingFresh[1] = false;
        window->updateDistance(Fix{});
    }
}

void MainController::handleEngageTracking(bool engaged) {
    if (engaged) {
        _currentMode = Tracking_Mode;
        _udpClient->sendMode(UdpClient::ModeID::TRACK);
        qDebug() << "[engage] armed -> TRACK";
    } else {
        for (int i = 0; i < 4 ;++i) {
            _speeds[i] = 0;
        }
        pushSpeeds();
        _currentMode = Test_Mode;
        qDebug() << "[engage] disengaged -> speed 0";
    }
}






