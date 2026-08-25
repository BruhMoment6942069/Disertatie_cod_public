//
// One camera's complete control surface: live view + its own pipeline bar.
// Written once, instantiated once per gimbal. Every signal carries the panel's
// camId, so the controller never has to guess which camera spoke.
//

#ifndef DISERTATIE_CAMERAPANEL_H
#define DISERTATIE_CAMERAPANEL_H
#pragma once

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QStringList>

class VideoView;
class QComboBox;
class QToolButton;

class CameraPanel : public QWidget {
    Q_OBJECT

public:
    explicit CameraPanel(int camId, QWidget* parent = nullptr);

    int camId() const { return _camId; }

    // driven by the controller
    void setFrame(const QImage& frame);
    void setFPS(double fps);
    void setDevices(const QStringList& devices);
    // worker reported a successful auto-calibration for THIS camera
    void calibrationFinished();

signals:
    // every one of these is addressed - first argument is always the camera id
    void cameraSelected(int camId, int deviceIndex);
    void exposureLockToggled(int camId, bool locked);
    void recordToggled(int camId, bool recording);
    void debugViewChanged(int camId, int view);
    void autoCalibrateToggled(int camId, bool on);
    void calibrationResetRequested(int camId);
    void measureModeToggled(int camId, bool on);
    void frameClicked(int camId, QPoint framePos);

private slots:
    // Named slots rather than lambdas, per project convention. Each one does the
    // same job: take the plain widget signal and stamp this panel's id onto it.
    void handleComboChanged(int deviceIndex);
    void handleExposureToggled(bool on);
    void handleRecordToggled(bool on);
    void handleDebugChanged(int view);
    void handleAutoCalibToggled(bool on);
    void handleResetClicked();
    void handleMeasureToggled(bool on);
    void handleViewClicked(QPoint framePos);

private:
    int _camId;
    VideoView*   _view        = nullptr;
    QComboBox*   _comboCamera = nullptr;
    QComboBox*   _comboDebug  = nullptr;
    QToolButton* _btnExposure = nullptr;
    QToolButton* _btnRecord   = nullptr;
    QToolButton* _btnAutoCalib = nullptr;
    QToolButton* _btnReset    = nullptr;
    QToolButton* _btnMeasure  = nullptr;
};

#endif //DISERTATIE_CAMERAPANEL_H
