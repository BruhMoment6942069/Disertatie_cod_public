//
// Created by Andrei on 20.02.2026.
//

#include "UdpClient.h"
#include <QThread>
#include <qcolor.h>
#include <QDebug>
#include <QNetworkInterface>
#include <QtEndian>


UdpClient::UdpClient(QObject *parent)
    : QObject(parent) {
    auto udpPort = 1234;
    _socket.bind(QHostAddress::AnyIPv4, udpPort);
    connect(&_socket, &QUdpSocket::readyRead, this, &UdpClient::ready_Read);
    _udpPort = udpPort;
    qDebug() << "UdpClient thread:" << QThread::currentThread();
    _keepAlive.setInterval(1500);
    connect(&_keepAlive, &QTimer::timeout, this, [this]() {
        if (_espAddress.isNull()) {
            return;
        }
        const char ka = 0x00;
        _socket.writeDatagram(&ka, 1, _espAddress, _udpPort);
    });
    _keepAlive.start();
}

void UdpClient::send_Message(const QString &message) {
    QByteArray data;
    data.append(message.toUtf8());

    // a fresh handshake means "rediscover" — drop the cached address so we
    // broadcast again and relearn it (handles switching networks).
    if (message.compare("Connected", Qt::CaseInsensitive) == 0) {
        _espAddress.clear();
    }

    if (_espAddress.isNull()) {
        // not discovered yet: broadcast the handshake so the ESP32 answers
        // from whatever DHCP address it currently has, on any subnet.
        for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
            if (!(iface.flags() & QNetworkInterface::IsUp)) {
                continue;
            }
            for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
                if (entry.ip().isLoopback()) {
                    continue;
                }
                QHostAddress bcast = entry.broadcast();
                if (!bcast.isNull()) {
                    _socket.writeDatagram(data, bcast, _udpPort);
                    qDebug() << "Broadcast to" << bcast.toString() << "on port:" << _udpPort;
                }
            }
        }

    } else {
        // already discovered: talk to it directly.
        _socket.writeDatagram(data, _espAddress, _udpPort);
        qDebug() << "Sent to" << _espAddress.toString() << "on port:" << _udpPort;
    }
}

void UdpClient::sendTarget(qint32 pan, qint32 tilt) {
    if (_espAddress.isNull()) {
        return;
    }
    QByteArray data(9,0);
    data[0] = static_cast<char>(MsgID::TARGET);
    qToLittleEndian<qint32>(pan, data.data() + 1);
    qToLittleEndian<qint32>(tilt, data.data() + 5);
    _socket.writeDatagram(data, _espAddress, _udpPort);
}

void UdpClient::sendMode(ModeID mode) {
    if (_espAddress.isNull()) {
        return;
    }
    QByteArray data(2,0);
    data[0] = static_cast<char>(MsgID::MODE);
    data[1] = static_cast<char>(mode);
    _socket.writeDatagram(data, _espAddress, _udpPort);
}

void UdpClient::ready_Read() {
    qDebug() << "Emitting from thread:" << QThread::currentThread();
    while (_socket.hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(_socket.pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;
        _socket.readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);

        if (QNetworkInterface::allAddresses().contains(sender)) {
            continue;
        }

        // remember where the ESP32 actually is, so we stop broadcasting and
        // unicast from now on. Survives IP changes: re-handshake -> re-learn.
        _espAddress = sender;

        qDebug() << " Message from: " << sender.toString();
        qDebug() << " Message port: " << senderPort;
        if (buffer.size() == 9 && buffer.compare("Connected", Qt::CaseInsensitive) == 0) {
            QString message = QString::fromUtf8(buffer);
            qDebug() << "emiting dataReceived: " << message;
            emit dataReceived(message);
            qDebug() << "done emiting";
            
        } else if (buffer.size() >= 2 && static_cast<quint8>(buffer[0]) == static_cast<quint8>(MsgID::MOTOR_POS)) {
            // Big endian MSB = most significant byte comes first
            // Little endian LSB = least significant byte comes first
            // let's say 0x12345678, the bytes are 12,34,56,78
            // 12 is MSB and 78 LSB
            quint8 count = static_cast<quint8>(buffer[1]);
            int expected = 2 + count * 4 + 1;
            if (buffer.size() >= expected) {
                QList<qint32> positions;
                for (quint8 i = 0 ; i < count; i++) {
                    qint32 pos = qFromLittleEndian<qint32>(buffer.constData() + 2 + i * 4);
                    positions.append(pos);
                }
                quint8 homed = static_cast<quint8>(buffer[2 + count * 4]);
                qDebug() << "positions: " << positions << "homed: " << homed;
                emit motorPositions(positions, homed);
            } else {
                // A short MOTOR_POS used to fall off the end of that if and disappear
                // - no emit, no log, no counter. The controller then held its last
                // _panSteps forever and went on solving from them, which is how the
                // rig reported a confident 1033mm at every distance for an evening.
                // A dropped packet must be noisier than a delivered one, not quieter.
                qWarning() << "[udp] MOTOR_POS DROPPED - claims" << count
                           << "motors so needs" << expected << "bytes, got"
                           << buffer.size() << ". Positions are now STALE.";
            }

        }



    }
}

void UdpClient::sendSpeed(const QList<qint32>& speeds) {
    if (_espAddress.isNull()) {
        static bool warned = false;
        if (!warned) {
            qWarning() << "[udp] sendSPEED DROPPED - no ESP address, press Start Connection";
            warned = true;
        }
        return;
    }
    QByteArray data(2 + speeds.size() * 4, 0);
    data[0] = static_cast<char>(MsgID::SPEED);
    data[1] = static_cast<char>(speeds.size());
    for (int i = 0; i < speeds.size(); ++i) {
        qToLittleEndian<qint32>(speeds[i], data.data() + 2 + i * 4);
    }
    _socket.writeDatagram(data, _espAddress, _udpPort);
}
