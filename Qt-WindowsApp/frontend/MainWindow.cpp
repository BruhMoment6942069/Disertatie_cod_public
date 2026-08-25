//
// Created by Andrei on 2/4/2026.
//

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QPushButton>
#include <QDebug>
#include <QStatusBar>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include "notification/NotificationPop.h"
#include "Animator.h"
#include <QDialog>
#include <QMessageBox>
#include "graph/graphData.h"
#include "../backend/vision/CameraEnumerator.h"
#include <QFileDialog>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    //UI!!!!!!
    ui->setupUi(this);
    ui->graphDataWidget->hide();
    ui->stackedWidget->setCurrentWidget(ui->emptyPage);
    ui->exposureLock1btn->setDefaultAction(ui->actionExposureC1_Lock);
    ui->comboCameras->addItems(CameraEnumerator::listCameras());
    ui->comboCameras->setCurrentIndex(-1);
    ui->comboCameras->setPlaceholderText("Select camera...");
    // second gimbal's camera. Same device list - the user picks a different entry.
    ui->comboCameras2->addItems(CameraEnumerator::listCameras());
    ui->comboCameras2->setCurrentIndex(-1);
    ui->comboCameras2->setPlaceholderText("Select camera 2...");
    ui->cameraRecodringCalib->setDefaultAction(ui->actionRecord);
    ui->comboDebugView->addItems({"Raw", "Mask", "Overlay"});
    ui->btnCalibReset->setDefaultAction(ui->actionCalibration_Reset);
    ui->btnAutoCalib->setDefaultAction(ui->actionAuto_Calibrate);
    ui->btnEngageTrack->setDefaultAction(ui->actionEngage_Tracking);
    ui->pipelineStrip->setEnabled(false);





    floatingNotify = new NotificationPop(ui->centralwidget);
    floatingNotify->setWindowFlags(Qt::Widget |Qt::FramelessWindowHint);

    notifyEffect = new QGraphicsOpacityEffect(floatingNotify);
    floatingNotify->setGraphicsEffect(notifyEffect);
    floatingNotify->setAttribute(Qt::WA_TranslucentBackground);
    floatingNotify->hide();

    auto* overlay = new QVBoxLayout(ui->videoView);
    overlay->addWidget(ui->comboCameras, 0, Qt::AlignLeft | Qt::AlignTop);
    overlay->addStretch(1);
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addWidget(ui->exposureLock1btn, 0, Qt::AlignLeft | Qt::AlignBottom);
    buttonRow->addWidget(ui->cameraRecodringCalib,0, Qt::AlignBottom);
    buttonRow->addStretch(1);

    overlay->addLayout(buttonRow);

    // Camera 2's area sits INSIDE the main window, beside the stacked widget, and is
    // shown/hidden on demand rather than being a floating window.
    // addWidget() reparents videoView2/comboCameras2 out of pageVideo automatically.
    _camera2Panel = new QWidget(this);
    auto* cam2Layout = new QVBoxLayout(_camera2Panel);
    cam2Layout->setContentsMargins(0, 0, 0, 0);
    cam2Layout->addWidget(ui->videoView2, 1);
    // Overlay the combo INSIDE the view, matching camera 1: a layout installed on the
    // view itself floats its children over the frame instead of stealing a row above it.
    auto* overlay2 = new QVBoxLayout(ui->videoView2);
    overlay2->addWidget(ui->comboCameras2, 0, Qt::AlignLeft | Qt::AlignTop);
    overlay2->addStretch(1);
    // The two views must sit in structurally IDENTICAL containers or they cannot come
    // out the same size. Camera 1 lives inside pageVideo's grid, which carries default
    // ~9px margins; camera 2's layout has none. Zero both so the views get the full
    // width and height of their grid column.
    ui->gridLayout_4->setContentsMargins(0, 0, 0, 0);
    ui->gridLayout_4->setSpacing(0);
    // camera 1 also sits one container deeper than camera 2 (inside the stacked widget),
    // so strip that layer's padding too
    ui->stackedWidget->setContentsMargins(0, 0, 0, 0);

    // Both views must be sized by the SAME rule or the grid splits the width unevenly.
    ui->videoView->setMinimumSize(320, 240);
    ui->videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->videoView2->setMinimumSize(320, 240);
    ui->videoView2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    ui->gridLayout_3->addWidget(_camera2Panel, 2, 4);
    // the pipeline bar controls BOTH cameras, so span it across both view columns
    ui->gridLayout_3->addWidget(ui->pipelineStrip, 1, 3, 1, 2);
    // Columns 3 and 4 split the width evenly once camera 2 is visible. Column 4's
    // stretch is toggled with the panel, otherwise the hidden panel still reserves space.
    ui->gridLayout_3->setColumnStretch(3, 1);
    ui->gridLayout_3->setColumnStretch(4, 0);
    _camera2Panel->hide();




    connect(ui->actionStart_Connection, &QAction::triggered, this, &MainWindow::connectionRequested);
    connect(ui->actionStart_Window, &QAction::triggered, this, &MainWindow::handleOpenWindowView);
    connect(ui->actionStart_Test_Sliders, &QAction::triggered, this, &MainWindow::handleTestSliders);
    connect(ui->sliderDataWidget, &SliderPanel::targetChanged, this, &MainWindow::targetRequested);
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &MainWindow::handleStackPageChanged);
    connect(ui->actionCamera_1, &QAction::triggered, this, &MainWindow::handleCamera1Open);
    connect(ui->actionCamera_2, &QAction::triggered, this, &MainWindow::handleCamera2Open);
    connect(ui->actionExposureC1_Lock, &QAction::toggled, this, &MainWindow::exposureC1LockToggled);
    connect(ui->comboCameras, &QComboBox::currentIndexChanged, this, &MainWindow::handleCamera0Selected);
    connect(ui->comboCameras2, &QComboBox::currentIndexChanged, this, &MainWindow::handleCamera1Selected);
    connect(ui->actionRecord, &QAction::toggled, this, &MainWindow::startCalibrationRecodring);
    connect(ui->actionOpenClip, &QAction::triggered, this, &MainWindow::handleOpenClip);
    connect(ui->comboDebugView, &QComboBox::currentIndexChanged, this, &MainWindow::debugViewChanged);
    connect(ui->videoView, &VideoView::frameClicked, this, &MainWindow::handleView0Clicked);
    connect(ui->videoView2, &VideoView::frameClicked, this, &MainWindow::handleView1Clicked);
    connect(ui->clipView, &VideoView::frameClicked, this, &MainWindow::handleView0Clicked);
    connect(ui->actionCalibration_Reset, &QAction::triggered, this, &MainWindow::calibrationResetRequested);
    connect(ui->actionAuto_Calibrate, &QAction::toggled, this, &MainWindow::autoCalibrateToggled);
    connect(ui->actionEngage_Tracking, &QAction::toggled, this, &MainWindow::engageTrackingToggled);


}

MainWindow::~MainWindow() {
    delete ui;
}
void MainWindow::showStatusMessage(const QString& message, int timeoutMs) {

}

void MainWindow::showNotification(const QString& message, int timeoutMs) {
    floatingNotify->setMessage(message);
    timeoutMs = 300;
    notifyEffect->setEnabled(true);

    int x = (ui->centralwidget->width() - floatingNotify->width()) / 2;
    int y = (ui->centralwidget->height() - floatingNotify->height()) - 20;

    floatingNotify->move(x,y);
    floatingNotify->show();
    floatingNotify->raise();

    Animator::fadeInOut(floatingNotify, notifyEffect, timeoutMs);

}

// Mirrors STEPS_PER_REV in the firmware's config.h - 1.8deg motor at 1/16 microstepping,
// so one step is 0.1125 deg.
static constexpr double STEPS_PER_REV_UI = 3200.0;

// The ESP counts steps and zeroSteps calibration is done in steps, so they stay on
// screen - but a bare "755" means nothing at a glance. Show the angle alongside.
//
// Deliberately NOT wrapped into +-360: with a 200 deg pan range, any reading outside
// that means the counter has drifted from skipped steps or the axis was never homed.
// Wrapping would disguise exactly the failure you most want to notice.
static QString stepsAsAngle(qint32 steps) {
    double deg = steps * 360.0 / STEPS_PER_REV_UI;
    return QString("%1° (%2)").arg(deg, 0, 'f', 1).arg(steps);
}

void MainWindow::updateCoordinates(const QList<qint32>& motorPosition) {
    ui->lblM1X1->setText(stepsAsAngle(motorPosition.value(0)));
    ui->lblM1Y1->setText(stepsAsAngle(motorPosition.value(1)));
    ui->lblM2X2->setText(stepsAsAngle(motorPosition.value(2)));
    ui->lblM2Y2->setText(stepsAsAngle(motorPosition.value(3)));

    if (_opGraphData) {
        ui->graphDataWidget->pushDataToBuffer(motorPosition);
    }

}

void MainWindow::updateDistance(const Fix &fix) {
    if (!fix.valid) {
        ui->lblDistance->setText("-");
        ui->lblPosX->setText("-");
        ui->lblPosY->setText("-");
        ui->lblPosZ->setText("-");
        ui->lblZDisagree->setText("-");
        return;
    }
    // fix.slant, NOT fix.distance. The rig reports a radius measured from the dot in
    // the plate, so the answer is the straight line through the air to the ball.
    // fix.distance is that line projected flat onto the plate - always shorter, and
    // by more the higher the ball sits. Reading it here under-reported every shot.
    ui->lblDistance->setText(QString::number(fix.slant, 'f', 0) + " mm");
    ui->lblPosX->setText(QString::number(fix.x, 'f', 0));
    ui->lblPosY->setText(QString::number(fix.y, 'f', 0));
    ui->lblPosZ->setText(QString::number(fix.z, 'f', 0));
    // The two heads each measure height independently, so their disagreement is a
    // free health check on tilt zeros, head level, and beam twist. Watch it.
    ui->lblZDisagree->setText(QString::number(fix.zDisagreement, 'f', 0));
}

void MainWindow::updateVideoFrame(int id, const QImage &frame) {
    if (id == 0) {
        ui->videoView->setFrame(frame);
    } else {
        ui->videoView2->setFrame(frame);
    }
}

void MainWindow::updateFPS(int id, const double &fps) {
    if (id == 0) {
        ui->videoView->setFPS(fps);
    } else {
        ui->videoView2->setFPS(fps);
    }
}

void MainWindow::handleCamera0Selected(int deviceIndex) {
    emit cameraSelected(0, deviceIndex);
}

void MainWindow::handleCamera1Selected(int deviceIndex) {
    emit cameraSelected(1, deviceIndex);
}

void MainWindow::updateClipFrame(const QImage &frame) {
    ui->clipView->setFrame(frame);
}

void MainWindow::handleAutoCalibrateFinished(int id) {
    if (id >= 0 && id < 2) {
        _calibDone[id] = true;
    }
    // Enable Engage as soon as ANY camera knows the ball, so working with a single
    // camera plugged in is not blocked.
    ui->actionEngage_Tracking->setEnabled(true);

    // But only untick the shared button once BOTH are done: unticking emits
    // toggled(false), which broadcasts setAutoCalibrate(false) to every worker and
    // would abort a camera that is still hunting for its stable circle.
    if (_calibDone[0] && _calibDone[1]) {
        ui->actionAuto_Calibrate->setChecked(false);
        _calibDone[0] = false;
        _calibDone[1] = false;
    }
}

void MainWindow::handleCamera2Open() {
    // Read real visibility rather than tracking a bool, so the toggle can never
    // desync from what is actually on screen.
    const bool show = !_camera2Panel->isVisible();
    _camera2Panel->setVisible(show);
    // give the column its share only while the panel is up, so camera 1 reclaims
    // the full width when camera 2 is hidden
    ui->gridLayout_3->setColumnStretch(4, show ? 1 : 0);
}

void MainWindow::handleView0Clicked(QPoint framePos) {
    emit frameSampleRequested(0, framePos);
}

void MainWindow::handleView1Clicked(QPoint framePos) {
    emit frameSampleRequested(1, framePos);
}

void MainWindow::handleOpenWindowView(){
    qDebug() << "OpenGL Window was opened";
    if (_opGraphData == false) {
        _opGraphData = true;
        ui->graphDataWidget->show();
        ui->graphDataWidget->update();
    } else if (_opGraphData == true) {
        _opGraphData = false;
        ui->graphDataWidget->clearData();
        ui->graphDataWidget->hide();
    }
}

void MainWindow::handleTestSliders() {
    //qDebug() << "Test sliders clicked - opening panel";
    if (ui->stackedWidget->currentWidget() == ui->pageSliders) {
        ui->stackedWidget->setCurrentWidget(ui->emptyPage);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->pageSliders);
    }
}

// PRE-HARDWARE #2: no final else -> emptyPage announces NOTHING, ESP keeps the previous
// mode (stale TEST with no panel on screen). Add a terminal else (likely Tracking_Mode).
void MainWindow::handleStackPageChanged() {
    bool videoish = (ui->stackedWidget->currentWidget() == ui->pageVideo
    || ui->stackedWidget->currentWidget() == ui->pageOpenVideo);
    ui->pipelineStrip->setEnabled(videoish);
    if (ui->stackedWidget->currentWidget() == ui->pageSliders) {
        emit(modeChanged(Test_Mode));
    }

}

void MainWindow::handleCamera1Open() {
    if (ui->stackedWidget->currentWidget() == ui->pageVideo) {
        ui->stackedWidget->setCurrentWidget(ui->emptyPage);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->pageVideo);
    }

}

void MainWindow::handleOpenClip() {
    QString path = QFileDialog::getOpenFileName(this, "Open clip", "../clips", "Clips (*.avi)");
    if (path.isEmpty()) {
        return;
    }
    if (ui->stackedWidget->currentWidget() == ui->pageOpenVideo) {
        ui->stackedWidget->setCurrentWidget(ui->emptyPage);
    } else {
        ui->stackedWidget->setCurrentWidget(ui->pageOpenVideo);
    }
    emit clipSelected(path);

}







