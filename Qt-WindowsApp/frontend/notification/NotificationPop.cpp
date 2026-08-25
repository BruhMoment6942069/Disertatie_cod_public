//
// Created by Andrei on 02.03.2026.
//

#include "NotificationPop.h"
#include <QWidget>

NotificationPop::NotificationPop(QWidget *parent) : QWidget(parent) {
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint );
    // 1. Setup UI elements
    notificationLabel = new QLabel(this);
    notificationLabel->setObjectName("notificationLabel");
    //notificationLabel->setAlignment(Qt::AlignCenter);
    //notificationLabel->setContentsMargins(0, 0, 0, 0);
    //notificationLabel->setStyleSheet("QLabel{background:black;}");

}
void NotificationPop::setMessage(const QString& message) {
    notificationLabel->setText(message);
}



