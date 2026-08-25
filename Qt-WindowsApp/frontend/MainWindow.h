//
// Created by Andrei on 2/4/2026.
//

#ifndef DISERTATIE_MAINWINDOW_H
#define DISERTATIE_MAINWINDOW_H
#pragma once
#include <QMainWindow>
#include <QGraphicsOpacityEffect>
#include <QStatusBar>
//#include "OpenGLwindow/openGLWindow.h"
#include "notification/NotificationPop.h"
#include "testSliders/SliderPanel.h"
#include "../backend/geometry/Triangulator.h"
#include <QImage>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showStatusMessage(const QString& message, int timeoutMs = 2000);
    void showNotification(const QString& message, int timeout = 2000);
    void updateCoordinates(const QList<qint32>& motorPosition);
    // id addresses the gimbal: 0 = first camera/view, 1 = second
    void updateVideoFrame(int id, const QImage& frame);
    void updateFPS(int id, const double& fps);
    void updateClipFrame(const QImage& frame);
    // An invalid Fix blanks the readout rather than leaving the last good number on
    // screen - a stale distance that still looks live is the worst kind of wrong.
    void updateDistance(const Fix& fix);


    signals:
    void actionButtonClicked();
    void startRequested();
    void resetRequested();
    void statusRequested();
    void connectionRequested();
    void targetRequested(qint32 pan, qint32 tilt);
    void modeChanged(int mode);
    void exposureC1LockToggled(bool lock);
    void cameraSelected(int camId, int deviceIndex);
    void startCalibrationRecodring(bool recording);
    void clipSelected(const QString& path);
    void debugViewChanged(int view);
    void frameSampleRequested(int camId, QPoint framePos);
    void calibrationResetRequested();
    void autoCalibrateToggled(bool toggled);
    void engageTrackingToggled(bool engaged);

public slots:
    void handleAutoCalibrateFinished(int id);

private slots:
    // one slot per combo so each can stamp its own camera id onto cameraSelected.
    // Named slots rather than lambdas, per the project's convention.
    void handleCamera0Selected(int deviceIndex);
    void handleCamera1Selected(int deviceIndex);
    void handleView0Clicked(QPoint framePos);
    void handleView1Clicked(QPoint framePos);
    void handleCamera2Open();
    void handleOpenWindowView();
    void handleTestSliders();
    void handleStackPageChanged();
    void handleCamera1Open();
    void handleOpenClip();


private:
    Ui::MainWindow *ui;
    QGraphicsOpacityEffect* notifyEffect;
    NotificationPop* floatingNotify;
    QTimer* renderTimer;
    bool _opGraphData = false;
    bool _sliderWindowOpen = false;
    // per-camera auto-calibration completion, so one camera finishing cannot
    // untick the shared button and abort the other camera mid-calibration
    bool _calibDone[2] = {false, false};
    // second camera's area, embedded in the main window beside the stacked widget;
    // toggled in and out of view rather than being a separate window
    QWidget* _camera2Panel = nullptr;
    // PRE-HARDWARE #3: MIRROR of MainController.h Modes — keep in sync!
    // Values are implicit and match by luck; make them explicit = 0, = 1, = 2 on BOTH.
    enum Modes {Test_Mode, Calibration_Mode, Tracking_Mode};
};



#endif //DISERTATIE_MAINWINDOW_H