//
// Created for the two-gimbal build: one panel per camera.
//

#include "CameraPanel.h"
#include "../video/VideoView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QToolButton>

// Built in code rather than a .ui file on purpose: the panel is instantiated from
// C++ once per camera, so there is no Designer placement to describe, and keeping
// the widget self-contained means adding a third camera costs one more `new`.
CameraPanel::CameraPanel(int camId, QWidget* parent)
    : QWidget(parent), _camId(camId)
{
    _view = new VideoView(this);

    _comboCamera = new QComboBox(this);
    _comboCamera->setCurrentIndex(-1);
    _comboCamera->setPlaceholderText(QString("Select camera %1...").arg(camId + 1));

    _comboDebug = new QComboBox(this);
    _comboDebug->addItems({"Raw", "Mask", "Overlay"});

    _btnExposure = new QToolButton(this);
    _btnExposure->setText("Exposure Lock");
    _btnExposure->setCheckable(true);

    _btnRecord = new QToolButton(this);
    _btnRecord->setText("Record");
    _btnRecord->setCheckable(true);

    _btnAutoCalib = new QToolButton(this);
    _btnAutoCalib->setText("Auto Calibrate");
    _btnAutoCalib->setCheckable(true);

    _btnReset = new QToolButton(this);
    _btnReset->setText("Calib Reset");

    _btnMeasure = new QToolButton(this);
    _btnMeasure->setText("Measure");
    _btnMeasure->setCheckable(true);

    auto* bar = new QHBoxLayout();
    bar->addWidget(_comboDebug);
    bar->addWidget(_btnReset);
    bar->addWidget(_btnAutoCalib);
    bar->addWidget(_btnMeasure);
    bar->addWidget(_btnExposure);
    bar->addWidget(_btnRecord);
    bar->addStretch(1);

    auto* root = new QVBoxLayout(this);
    root->addWidget(_comboCamera, 0, Qt::AlignLeft);
    root->addWidget(_view, 1);
    root->addLayout(bar);

    connect(_comboCamera, &QComboBox::currentIndexChanged, this, &CameraPanel::handleComboChanged);
    connect(_comboDebug, &QComboBox::currentIndexChanged, this, &CameraPanel::handleDebugChanged);
    connect(_btnExposure, &QToolButton::toggled, this, &CameraPanel::handleExposureToggled);
    connect(_btnRecord, &QToolButton::toggled, this, &CameraPanel::handleRecordToggled);
    connect(_btnAutoCalib, &QToolButton::toggled, this, &CameraPanel::handleAutoCalibToggled);
    connect(_btnReset, &QToolButton::clicked, this, &CameraPanel::handleResetClicked);
    connect(_btnMeasure, &QToolButton::toggled, this, &CameraPanel::handleMeasureToggled);
    connect(_view, &VideoView::frameClicked, this, &CameraPanel::handleViewClicked);
}

void CameraPanel::setFrame(const QImage& frame) {
    _view->setFrame(frame);
}

void CameraPanel::setFPS(double fps) {
    _view->setFPS(fps);
}

void CameraPanel::setDevices(const QStringList& devices) {
    _comboCamera->clear();
    _comboCamera->addItems(devices);
    _comboCamera->setCurrentIndex(-1);
}

void CameraPanel::calibrationFinished() {
    // Untick only THIS panel's button. With one shared action across both cameras,
    // the first camera to finish would emit toggled(false) and abort the other
    // camera's still-running calibration.
    _btnAutoCalib->setChecked(false);
}

// ─── id stamping ─────────────────────────────────────────────────────────────

void CameraPanel::handleComboChanged(int deviceIndex) {
    emit cameraSelected(_camId, deviceIndex);
}

void CameraPanel::handleExposureToggled(bool on) {
    emit exposureLockToggled(_camId, on);
}

void CameraPanel::handleRecordToggled(bool on) {
    emit recordToggled(_camId, on);
}

void CameraPanel::handleDebugChanged(int view) {
    emit debugViewChanged(_camId, view);
}

void CameraPanel::handleAutoCalibToggled(bool on) {
    emit autoCalibrateToggled(_camId, on);
}

void CameraPanel::handleResetClicked() {
    emit calibrationResetRequested(_camId);
}

void CameraPanel::handleMeasureToggled(bool on) {
    emit measureModeToggled(_camId, on);
}

void CameraPanel::handleViewClicked(QPoint framePos) {
    emit frameClicked(_camId, framePos);
}
