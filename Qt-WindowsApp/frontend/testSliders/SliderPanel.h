//
// Created by Andrei on 19.06.2026.
//

#ifndef QT_WINDOWSAPP_SLIDERPANEL_H
#define QT_WINDOWSAPP_SLIDERPANEL_H

#pragma once
#include <QWidget>
#include <QSlider>

class SliderPanel : public QWidget {
    Q_OBJECT

public:
    explicit SliderPanel(QWidget *parent = nullptr);

    signals:
    void targetChanged(qint32 pan, qint32 tilt);

    private slots:
    void onSliderMoved();

private:
    QSlider* _panSlider;
    QSlider* _tiltSlider;
};

#endif //QT_WINDOWSAPP_SLIDERPANEL_H
