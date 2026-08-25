//
// Created by Andrei on 2/4/2026.
//

#include "AppService.h"
#include <iostream>
#include <QDebug>
#include "../Network/UdpClient.h"


void AppService::doSomething() {
    qDebug() << "[Backend] Doing the thing!\n";
}
void AppService::startProcess() {
    qDebug() << "[Backend] Process started\n";

}

void AppService::resetProcess() {
    qDebug() << "[Backend] Process reset\n";
}

void AppService::printStatus() {
    qDebug() << "[Backend] Status: everything is fine\n";
}
void AppService::connectionStarted() {
    qDebug() << "[Backend] Attempting to connect to esp32\n";
}






