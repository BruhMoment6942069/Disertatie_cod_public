//
// Created by Andrei on 09.02.2026.
//

#ifndef DISERTATIE_ANIMATOR_H
#define DISERTATIE_ANIMATOR_H
#pragma once
#include <QWidget>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>


class Animator {
public:
    //Fade in/out a widget with a message
    static void fadeInOut(QWidget* widget,
        QGraphicsOpacityEffect* effect,
        int displayMs = 3000,
        int fadeMs = 300
        );
};


#endif //DISERTATIE_ANIMATOR_H