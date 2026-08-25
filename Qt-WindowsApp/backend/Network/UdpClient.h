//
// Created by Andrei on 20.02.2026.
//
#ifndef DISERTATIE_UDPCLIENT_H
#define DISERTATIE_UDPCLIENT_H



#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QList>

class UdpClient : public QObject {
    Q_OBJECT

public:
    explicit UdpClient(QObject* parent = nullptr);
    void send_Message(const QString& message);
    void sendTarget(qint32 pan, qint32 tilt);
    enum class MsgID : quint8 {MOTOR_POS = 1, TARGET = 2, MODE = 3, SPEED = 4};
    // WIRE LAW: these MUST match ProiectDisertatie/include/config.h.
    // TRACK/TEST are the only two the ESP knows (MODE_TRACK=0, MODE_TEST=1); its mode-follow
    // is `(g_mode == MODE_TEST) ? TEST_MODE : T_MOVEMENT`, so ANY other value lands the motors
    // in tracking. CALIB is PC-side only and is never emitted now that mode arming is an
    // explicit button - it is parked at 2 purely so the enum stays exhaustive.
    enum class ModeID : quint8 {TRACK = 0, TEST = 1, CALIB = 2, };
    void sendMode(ModeID mode);
    void sendSpeed(const QList<qint32>& speeds);

signals:
    void dataReceived(const QString &message);
    void motorPositions(const QList<qint32> &positions, bool homed);


private slots:
    void ready_Read();


private:
    QUdpSocket _socket;
    int _udpPort;
    QHostAddress _espAddress;   // discovered ESP32 address; invalid until first reply
    QTimer _keepAlive;

};
#endif //DISERTATIE_UDPCLIENT_H