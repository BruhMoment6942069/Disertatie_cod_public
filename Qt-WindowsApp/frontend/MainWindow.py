/********************************************************************************
** Form generated from reading UI file 'MainWindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>
#include "graph/graphData.h"
#include "testSliders/SliderPanel.h"
#include "video/VideoView.h"

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionEsp32;
    QAction *actionStart_Connection;
    QAction *actionStart_Window;
    QAction *actionTest_Sliders;
    QAction *actionStart_Test_Sliders;
    QAction *actionCamera_1;
    QAction *actionExposureC1_Lock;
    QAction *actionRecord;
    QAction *actionOpenClip;
    QAction *actionCalibration_Reset;
    QAction *actionAuto_Calibrate;
    QAction *actionEngage_Tracking;
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QFrame *frame;
    QGridLayout *gridLayout_3;
    graphData *graphDataWidget;
    QSpacerItem *verticalSpacer;
    QGroupBox *grpBCOORDS;
    QGridLayout *gridLayout_2;
    QLabel *lblM1Y1;
    QLabel *lblM2Y2;
    QLabel *label_5;
    QLabel *lblM1X1;
    QLabel *lblM2X2;
    QLabel *label_6;
    QLabel *label;
    QLabel *label_4;
    QLabel *label_2;
    QLabel *label_3;
    QStackedWidget *stackedWidget;
    QWidget *pageSliders;
    QGridLayout *gridLayout_5;
    SliderPanel *sliderDataWidget;
    QWidget *emptyPage;
    QWidget *pageVideo;
    QGridLayout *gridLayout_4;
    VideoView *videoView;
    QToolButton *exposureLock1btn;
    QComboBox *comboCameras;
    QToolButton *cameraRecodringCalib;
    QWidget *pageOpenVideo;
    QGridLayout *gridLayout_6;
    VideoView *clipView;
    QWidget *pipelineStrip;
    QHBoxLayout *horizontalLayout_2;
    QComboBox *comboDebugView;
    QToolButton *btnCalibReset;
    QToolButton *btnAutoCalib;
    QToolButton *btnEngageTrack;
    QSpacerItem *horizontalSpacer;
    QMenuBar *menubar;
    QMenu *menuConnect;
    QMenu *btn_CoordView;
    QMenu *Test_Sliders;
    QMenu *menuCameraView;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->setEnabled(true);
        MainWindow->resize(718, 697);
        actionEsp32 = new QAction(MainWindow);
        actionEsp32->setObjectName("actionEsp32");
        actionStart_Connection = new QAction(MainWindow);
        actionStart_Connection->setObjectName("actionStart_Connection");
        actionStart_Connection->setCheckable(false);
        actionStart_Window = new QAction(MainWindow);
        actionStart_Window->setObjectName("actionStart_Window");
        actionTest_Sliders = new QAction(MainWindow);
        actionTest_Sliders->setObjectName("actionTest_Sliders");
        actionTest_Sliders->setMenuRole(QAction::MenuRole::NoRole);
        actionStart_Test_Sliders = new QAction(MainWindow);
        actionStart_Test_Sliders->setObjectName("actionStart_Test_Sliders");
        actionCamera_1 = new QAction(MainWindow);
        actionCamera_1->setObjectName("actionCamera_1");
        actionExposureC1_Lock = new QAction(MainWindow);
        actionExposureC1_Lock->setObjectName("actionExposureC1_Lock");
        actionExposureC1_Lock->setCheckable(true);
        actionRecord = new QAction(MainWindow);
        actionRecord->setObjectName("actionRecord");
        actionRecord->setCheckable(true);
        actionRecord->setMenuRole(QAction::MenuRole::NoRole);
        actionOpenClip = new QAction(MainWindow);
        actionOpenClip->setObjectName("actionOpenClip");
        actionOpenClip->setMenuRole(QAction::MenuRole::NoRole);
        actionCalibration_Reset = new QAction(MainWindow);
        actionCalibration_Reset->setObjectName("actionCalibration_Reset");
        actionCalibration_Reset->setMenuRole(QAction::MenuRole::NoRole);
        actionAuto_Calibrate = new QAction(MainWindow);
        actionAuto_Calibrate->setObjectName("actionAuto_Calibrate");
        actionAuto_Calibrate->setCheckable(true);
        actionAuto_Calibrate->setMenuRole(QAction::MenuRole::NoRole);
        actionEngage_Tracking = new QAction(MainWindow);
        actionEngage_Tracking->setObjectName("actionEngage_Tracking");
        actionEngage_Tracking->setCheckable(true);
        actionEngage_Tracking->setEnabled(false);
        actionEngage_Tracking->setMenuRole(QAction::MenuRole::NoRole);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName("horizontalLayout");
        frame = new QFrame(centralwidget);
        frame->setObjectName("frame");
        frame->setEnabled(true);
        frame->setFrameShape(QFrame::Shape::StyledPanel);
        frame->setFrameShadow(QFrame::Shadow::Raised);
        gridLayout_3 = new QGridLayout(frame);
        gridLayout_3->setObjectName("gridLayout_3");
        graphDataWidget = new graphData(frame);
        graphDataWidget->setObjectName("graphDataWidget");
        graphDataWidget->setMinimumSize(QSize(40, 40));

        gridLayout_3->addWidget(graphDataWidget, 0, 3, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        gridLayout_3->addItem(verticalSpacer, 2, 0, 1, 1);

        grpBCOORDS = new QGroupBox(frame);
        grpBCOORDS->setObjectName("grpBCOORDS");
        grpBCOORDS->setMaximumSize(QSize(150, 100));
        gridLayout_2 = new QGridLayout(grpBCOORDS);
        gridLayout_2->setObjectName("gridLayout_2");
        lblM1Y1 = new QLabel(grpBCOORDS);
        lblM1Y1->setObjectName("lblM1Y1");

        gridLayout_2->addWidget(lblM1Y1, 1, 2, 1, 1);

        lblM2Y2 = new QLabel(grpBCOORDS);
        lblM2Y2->setObjectName("lblM2Y2");

        gridLayout_2->addWidget(lblM2Y2, 3, 2, 1, 1);

        label_5 = new QLabel(grpBCOORDS);
        label_5->setObjectName("label_5");

        gridLayout_2->addWidget(label_5, 2, 1, 1, 1);

        lblM1X1 = new QLabel(grpBCOORDS);
        lblM1X1->setObjectName("lblM1X1");

        gridLayout_2->addWidget(lblM1X1, 0, 2, 1, 1);

        lblM2X2 = new QLabel(grpBCOORDS);
        lblM2X2->setObjectName("lblM2X2");

        gridLayout_2->addWidget(lblM2X2, 2, 2, 1, 1);

        label_6 = new QLabel(grpBCOORDS);
        label_6->setObjectName("label_6");

        gridLayout_2->addWidget(label_6, 3, 1, 1, 1);

        label = new QLabel(grpBCOORDS);
        label->setObjectName("label");

        gridLayout_2->addWidget(label, 0, 0, 1, 1);

        label_4 = new QLabel(grpBCOORDS);
        label_4->setObjectName("label_4");

        gridLayout_2->addWidget(label_4, 2, 0, 1, 1);

        label_2 = new QLabel(grpBCOORDS);
        label_2->setObjectName("label_2");

        gridLayout_2->addWidget(label_2, 1, 1, 1, 1);

        label_3 = new QLabel(grpBCOORDS);
        label_3->setObjectName("label_3");

        gridLayout_2->addWidget(label_3, 0, 1, 1, 1);


        gridLayout_3->addWidget(grpBCOORDS, 0, 0, 1, 1);

        stackedWidget = new QStackedWidget(frame);
        stackedWidget->setObjectName("stackedWidget");
        pageSliders = new QWidget();
        pageSliders->setObjectName("pageSliders");
        gridLayout_5 = new QGridLayout(pageSliders);
        gridLayout_5->setObjectName("gridLayout_5");
        sliderDataWidget = new SliderPanel(pageSliders);
        sliderDataWidget->setObjectName("sliderDataWidget");
        sliderDataWidget->setEnabled(true);

        gridLayout_5->addWidget(sliderDataWidget, 0, 0, 1, 1);

        stackedWidget->addWidget(pageSliders);
        emptyPage = new QWidget();
        emptyPage->setObjectName("emptyPage");
        stackedWidget->addWidget(emptyPage);
        pageVideo = new QWidget();
        pageVideo->setObjectName("pageVideo");
        gridLayout_4 = new QGridLayout(pageVideo);
        gridLayout_4->setObjectName("gridLayout_4");
        videoView = new VideoView(pageVideo);
        videoView->setObjectName("videoView");
        exposureLock1btn = new QToolButton(videoView);
        exposureLock1btn->setObjectName("exposureLock1btn");
        exposureLock1btn->setGeometry(QRect(10, 460, 144, 20));
        comboCameras = new QComboBox(videoView);
        comboCameras->setObjectName("comboCameras");
        comboCameras->setGeometry(QRect(20, 20, 82, 26));
        cameraRecodringCalib = new QToolButton(videoView);
        cameraRecodringCalib->setObjectName("cameraRecodringCalib");
        cameraRecodringCalib->setGeometry(QRect(170, 460, 111, 20));
        cameraRecodringCalib->setCheckable(true);

        gridLayout_4->addWidget(videoView, 1, 1, 1, 1);

        stackedWidget->addWidget(pageVideo);
        pageOpenVideo = new QWidget();
        pageOpenVideo->setObjectName("pageOpenVideo");
        gridLayout_6 = new QGridLayout(pageOpenVideo);
        gridLayout_6->setObjectName("gridLayout_6");
        clipView = new VideoView(pageOpenVideo);
        clipView->setObjectName("clipView");

        gridLayout_6->addWidget(clipView, 1, 0, 1, 1);

        stackedWidget->addWidget(pageOpenVideo);

        gridLayout_3->addWidget(stackedWidget, 2, 3, 1, 1);

        pipelineStrip = new QWidget(frame);
        pipelineStrip->setObjectName("pipelineStrip");
        horizontalLayout_2 = new QHBoxLayout(pipelineStrip);
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        comboDebugView = new QComboBox(pipelineStrip);
        comboDebugView->setObjectName("comboDebugView");

        horizontalLayout_2->addWidget(comboDebugView);

        btnCalibReset = new QToolButton(pipelineStrip);
        btnCalibReset->setObjectName("btnCalibReset");

        horizontalLayout_2->addWidget(btnCalibReset);

        btnAutoCalib = new QToolButton(pipelineStrip);
        btnAutoCalib->setObjectName("btnAutoCalib");

        horizontalLayout_2->addWidget(btnAutoCalib);

        btnEngageTrack = new QToolButton(pipelineStrip);
        btnEngageTrack->setObjectName("btnEngageTrack");

        horizontalLayout_2->addWidget(btnEngageTrack);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);


        gridLayout_3->addWidget(pipelineStrip, 1, 3, 1, 1);


        horizontalLayout->addWidget(frame);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 718, 21));
        menuConnect = new QMenu(menubar);
        menuConnect->setObjectName("menuConnect");
        btn_CoordView = new QMenu(menubar);
        btn_CoordView->setObjectName("btn_CoordView");
        Test_Sliders = new QMenu(menubar);
        Test_Sliders->setObjectName("Test_Sliders");
        menuCameraView = new QMenu(menubar);
        menuCameraView->setObjectName("menuCameraView");
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        menubar->addAction(menuConnect->menuAction());
        menubar->addAction(btn_CoordView->menuAction());
        menubar->addAction(Test_Sliders->menuAction());
        menubar->addAction(menuCameraView->menuAction());
        menuConnect->addSeparator();
        menuConnect->addSeparator();
        menuConnect->addAction(actionStart_Connection);
        btn_CoordView->addAction(actionStart_Window);
        Test_Sliders->addAction(actionStart_Test_Sliders);
        menuCameraView->addAction(actionCamera_1);
        menuCameraView->addAction(actionOpenClip);

        retranslateUi(MainWindow);

        stackedWidget->setCurrentIndex(2);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        actionEsp32->setText(QCoreApplication::translate("MainWindow", "Esp32", nullptr));
        actionStart_Connection->setText(QCoreApplication::translate("MainWindow", "Start Connection", nullptr));
        actionStart_Window->setText(QCoreApplication::translate("MainWindow", "Start Window", nullptr));
        actionTest_Sliders->setText(QCoreApplication::translate("MainWindow", "Test_Sliders", nullptr));
        actionStart_Test_Sliders->setText(QCoreApplication::translate("MainWindow", "Start Test Sliders", nullptr));
        actionCamera_1->setText(QCoreApplication::translate("MainWindow", "Camera 1", nullptr));
        actionExposureC1_Lock->setText(QCoreApplication::translate("MainWindow", "Camera 1 Exposure Lock", nullptr));
        actionRecord->setText(QCoreApplication::translate("MainWindow", "Record", nullptr));
        actionOpenClip->setText(QCoreApplication::translate("MainWindow", "OpenClip", nullptr));
        actionCalibration_Reset->setText(QCoreApplication::translate("MainWindow", "Reset Calibration", nullptr));
        actionAuto_Calibrate->setText(QCoreApplication::translate("MainWindow", "Auto_Calibrate", nullptr));
        actionEngage_Tracking->setText(QCoreApplication::translate("MainWindow", "Engage Tracking", nullptr));
        grpBCOORDS->setTitle(QCoreApplication::translate("MainWindow", "Coordinates", nullptr));
        lblM1Y1->setText(QString());
        lblM2Y2->setText(QString());
        label_5->setText(QCoreApplication::translate("MainWindow", "X1:", nullptr));
        lblM1X1->setText(QString());
        lblM2X2->setText(QString());
        label_6->setText(QCoreApplication::translate("MainWindow", "Y2:", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "Motor 1", nullptr));
        label_4->setText(QCoreApplication::translate("MainWindow", "Motor 2", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "Y2:", nullptr));
        label_3->setText(QCoreApplication::translate("MainWindow", "X1:", nullptr));
        exposureLock1btn->setText(QCoreApplication::translate("MainWindow", "Camera 1 Exposure Lock", nullptr));
        cameraRecodringCalib->setText(QCoreApplication::translate("MainWindow", "Record", nullptr));
        btnCalibReset->setText(QCoreApplication::translate("MainWindow", "Calibration Reset", nullptr));
        btnAutoCalib->setText(QCoreApplication::translate("MainWindow", "Auto Calibration", nullptr));
        btnEngageTrack->setText(QCoreApplication::translate("MainWindow", "Engage Tracking", nullptr));
        menuConnect->setTitle(QCoreApplication::translate("MainWindow", "Connect", nullptr));
        btn_CoordView->setTitle(QCoreApplication::translate("MainWindow", "CoordinatesView", nullptr));
        Test_Sliders->setTitle(QCoreApplication::translate("MainWindow", "TestSliders", nullptr));
        menuCameraView->setTitle(QCoreApplication::translate("MainWindow", "CameraView", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // MAINWINDOW_H
