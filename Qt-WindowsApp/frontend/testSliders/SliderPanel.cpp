//
// Created by Andrei on 19.06.2026.
//

#include "SliderPanel.h"
#include <QVBoxLayout>

SliderPanel::SliderPanel(QWidget *parent) : QWidget(parent) {
    _panSlider = new QSlider(Qt::Horizontal, this);
    _tiltSlider = new QSlider(Qt::Horizontal, this);
    _panSlider->setRange(0,60);
    _tiltSlider->setRange(0, 200);
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(_panSlider);
    layout->addWidget(_tiltSlider);
    this->setLayout(layout);

    connect(_panSlider, &QSlider::valueChanged, this, &SliderPanel::onSliderMoved);
    connect(_tiltSlider, &QSlider::valueChanged, this, &SliderPanel::onSliderMoved);


}

void SliderPanel::onSliderMoved() {
    emit targetChanged(_panSlider->value(), _tiltSlider->value());
}
