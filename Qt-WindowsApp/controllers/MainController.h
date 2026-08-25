//
// Created by Andrei on 2/4/2026.
//

#ifndef DISERTATIE_MAINCONTROLLER_H
#define DISERTATIE_MAINCONTROLLER_H
#pragma once
#include <QObject>
#include "../frontend/notification/NotificationPop.h"
#include "../backend/Network/UdpClient.h"
#include <QThread>
#include "../backend/vision/VisionWorker.h"
#include "../backend/geometry/Triangulator.h"
#include <QImage>
#include <QTimer>

class MainWindow;
class AppService;

class MainController : public QObject {
    Q_OBJECT

public:
    explicit MainController(MainWindow* window);
    ~MainController();

public slots:
    void handleNotification(const QString& message);
    void handleCameraSelected(int camId, int deviceIndex);


private slots:
    void onActionButton();

    void handleStart();

    void handleReset();

    void handleStatus();

    void handleConnection();
    void handleMotorPositions(const QList<qint32>& motorPositions, bool homed);
    void handleTargetSliders(qint32 pan, qint32 tilt);
    void handleModeSent(int mode);
    void handleFrameReady(int id, const QImage& image);
    void handleBallTracked(int id,bool locked, double errX, double errY);
    void handleEngageTracking(bool engaged);
    void handleCameraFailed(const QString& reason);
    void handleFPSUpdate(int id, const double& fps);
    void handleClipsSelected(const QString& path);
    // Blanks the readout when a camera stops reporting entirely (unplugged, crashed).
    // Losing lock is handled inline; total silence produces no callbacks at all, so
    // only a timer can notice it.
    void handleFixTimeout();

signals:
    // These are broadcast to BOTH workers; camId is the address, and each worker
    // ignores anything not aimed at it. Keeps one signal per action instead of
    // per-worker connections that could drift apart.
    void captureStartRequested(int camId, int deviceIndex);
    void captureStopRequested(int camId);
    void playBackStartRequested(int camId, const QString& path);



private:
    MainWindow* window;
    AppService* service;
    UdpClient* _udpClient;
    NotificationPop* _notificationPop;
    bool _isAlreadyConnected = false;
    QThread* _visionThread[2] = {nullptr, nullptr};
    VisionWorker* _visionWorker[2] = {nullptr, nullptr};
    bool _playingClip = false;
    // PRE-HARDWARE #3: MIRROR of MainWindow.h Modes — keep in sync! Explicit values pending.
    enum Modes {Test_Mode, Calibration_Mode, Tracking_Mode};
    int _currentMode = Test_Mode;
    qint32 _speeds[4] = {0, 0, 0, 0};
    void pushSpeeds();
    void setupVisionWorker(int id);

    // ── triangulation ────────────────────────────────────────────────────────
    // The two bearings arrive independently (one per camera, ~30Hz each) and the
    // motor positions on their own 50Hz stream. Everything here exists to pair them
    // up honestly: solve only when BOTH cameras have reported since the last fix,
    // and refuse when the pair is not simultaneous enough to mean anything.
    qint32   _panSteps[2]     = {0, 0};      // positions[0], positions[2]
    qint32   _tiltSteps[2]    = {0, 0};      // positions[1], positions[3]
    bool     _homed           = false;       // unhomed steps have an arbitrary origin
    Bearing  _bearing[2]      = {};
    bool     _bearingFresh[2] = {false, false};
    qint64   _bearingMs[2]    = {0, 0};
    // When the MOTOR_POS packet last arrived. Starts at 0 so nothing solves until the
    // ESP has actually spoken - an unset clock must read "ancient", never "now".
    qint64   _posMs           = 0;
    // Last errX/errY actually accepted, per camera. On a detection MISS the tracker
    // stays Locked (miss hysteresis) and re-emits the previous centre unchanged - so
    // without this the freshness gate is satisfied by data that never moved.
    double   _lastErrX[2]     = {0.0, 0.0};
    double   _lastErrY[2]     = {0.0, 0.0};
    bool     _haveLastErr[2]  = {false, false};
    // Consecutive coasted repeats per camera. Drives the speed decay in
    // handleBallTracked, and printed so a dropout is visible in the log instead of
    // looking like a normal command that happens to repeat.
    int      _repeatCount[2]  = {0, 0};
    RigConfig _rig;
    QTimer   _fixWatchdog;
    void tryTriangulate();
};


#endif //DISERTATIE_MAINCONTROLLER_H