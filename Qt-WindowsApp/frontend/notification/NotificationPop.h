//
// Created by Andrei on 02.03.2026.
//


#ifndef DISERTATIE_NOTIFICATIONPOP_H
#define DISERTATIE_NOTIFICATIONPOP_H
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
class NotificationPop : public QWidget {
    Q_OBJECT

public:
    explicit NotificationPop(QWidget* parent = nullptr);
    void setMessage(const QString& message);

//private slots:
signals:
    void getMessage(const QString& message);
private:
    QLabel* notificationLabel;

};
#endif //DISERTATIE_NOTIFICATIONPOP_H