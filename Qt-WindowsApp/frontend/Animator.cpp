//
// Created by Andrei on 09.02.2026.
//

#include "Animator.h"
#include <QTimer>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>

void Animator::fadeInOut(QWidget *widget,
    QGraphicsOpacityEffect *effect,
    const int displayMs,
    const int fadeMs) {
    effect->setOpacity(0.0);
    widget->show();

    auto* fadeIn = new QPropertyAnimation(effect, "opacity");
    fadeIn->setDuration(fadeMs);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    auto* fadeOut = new QPropertyAnimation(effect, "opacity");
    fadeOut->setDuration(fadeMs);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    auto* group = new QSequentialAnimationGroup(widget);
    group->addAnimation(fadeIn);
    group->addPause(displayMs);
    group->addAnimation(fadeOut);

    QObject::connect(group, &QSequentialAnimationGroup::finished, [widget]() {widget->hide();});

    group->start(QAbstractAnimation::DeleteWhenStopped);


}
