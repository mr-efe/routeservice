#ifndef UDPSOCKET_H
#define UDPSOCKET_H

#include <QUdpSocket>

class UdpSocket : public QUdpSocket
{
    Q_OBJECT

public:
#ifdef MEMORY_DEBUGGING_ENABLED
    static int CTOR_COUNT;
    static int DTOR_COUNT;
#endif

public:
    UdpSocket(QObject* parent = 0);
    virtual ~UdpSocket();

signals:
    void interfaceChanged();
signals:
    void routeError();

public:
    bool joinMulticastGroup(const QHostAddress &groupAddress);
    bool leaveMulticastGroup(const QHostAddress &groupAddress);

    qint64 writeDatagram(const char* data,int size, const QHostAddress &host, quint16 port);

    inline qint64 writeDatagram(const QByteArray &datagram, const QHostAddress &host, quint16 port)
        { return writeDatagram(datagram.constData(), datagram.size(), host, port); }


private:
    bool leaveMulticastGroup(const QHostAddress &groupAddress,
                             const QNetworkInterface &iface);
    bool joinMulticastGroup(const QHostAddress &groupAddress,
                            const QNetworkInterface &iface);

    void timerEvent( QTimerEvent*e);
    inline void checkAddresses();

    int mTimer = 0;
    quint32 mChecksum = 0;

    QList<QHostAddress> mMulticastAddrs;
};

#endif // UDPSOCKET_H
