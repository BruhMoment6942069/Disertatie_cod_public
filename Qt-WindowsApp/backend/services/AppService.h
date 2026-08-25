//
// Created by Andrei on 2/4/2026.
//

#ifndef DISERTATIE_APPSERVICE_H
#define DISERTATIE_APPSERVICE_H
#pragma once
#include <QObject>
#include <QString>

class AppService : public QObject {
    Q_OBJECT
public:
    void doSomething();
    void startProcess();
    void resetProcess();
    void printStatus();
    void connectionStarted();

    signals:

private:
};

#endif //DISERTATIE_APPSERVICE_H