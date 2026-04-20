#ifndef MACDSERVICE_H
#define MACDSERVICE_H

#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QSet>
#include "udpsocket.h"

class MacDService : public QObject
{
    Q_OBJECT
public:
    explicit MacDService(QObject *parent = nullptr);
     ~MacDService();
signals:
    void peerDiscovered(const QHostAddress& ip, uint networkInterfaceIndex);
private slots:
    void onNewTcpConnection();
    void onInterfaceChanged();
    void sendMacD();
    void onTcpPollTimer();
private:
    void checkViaExtendedTcpTable();
    inline static const QHostAddress MULTICAST_ADDR = QHostAddress("224.0.0.1");
    static const quint16 MACD_UDP_PORT = 4587;
    static const quint16 MACD_TCP_PORT = 4588;
private:
    UdpSocket mUdpSocket;
    QTcpServer mTcpServer;
    QTimer mSendMacDTimer;
    QTimer mTcpPollTimer;
    QSet<QString> mKnownTcpConnections;
};

#endif // MACDSERVICE_H
