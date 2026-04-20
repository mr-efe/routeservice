#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <QUdpSocket>
#include <QHostAddress>
#include <QTimer>
#include <QObject>
#include "UdpSocket.h"

class DiscoveryService : public QObject
{
    Q_OBJECT
public:
    DiscoveryService();
    virtual ~DiscoveryService();
signals:
    void interfaceChangd();
    void peerDiscovered(const QHostAddress& ip, uint networkInterfaceIndex);
private slots:
    void onReadyRead();
private slots:
    void socketError();
    void datagramReceived();
public:
    const quint32 BootThreshold= 2;
    const quint32 FastDiscoveryThreshold= 4;

    const int FastDiscoveryInterval= 15000;
    const int DiscoveryInterval= 15000;

    const int MaxPeerTimerValue = 1000;
    const int MinPeerTimerValue = 100;
    const int IdlePeerTimerValue = 50;
private:
    inline static const QHostAddress MULTICAST_ADDR = QHostAddress("224.0.0.1");
    static const quint16 MACD_UDP_PORT = 18807;
private:
    UdpSocket   mSocket;
    QTimer mTimer;

    quint8 mDcnt;
    quint8 mResponseInterval;

    QTimer mDiscoveryResponseTimer;

    bool mRunning;
    quint16 mProjectId;
};

#endif // DISCOVERYSERVICE_H
