#include "discoveryservice.h"

#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QDataStream>
#include <QNetworkDatagram>
#include <QDateTime>

DiscoveryService::DiscoveryService()
{
    connect(&mSocket, SIGNAL(interfaceChanged()), this, SIGNAL(interfaceChangd()));
    connect(&mSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(&mSocket, SIGNAL(errorOccurred(QAbstractSocket::SocketError)), this, SLOT(socketError()));

    if(!mSocket.bind(QHostAddress::AnyIPv4, MACD_UDP_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCritical() << "could not bind to address!";
    }

    mSocket.setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
    if(!mSocket.joinMulticastGroup(MULTICAST_ADDR)) {
        qCritical() << "Failed to join multicast group!";
    }
}

DiscoveryService::~DiscoveryService()
{
    mSocket.leaveMulticastGroup(MULTICAST_ADDR);
    mSocket.close();
}

void DiscoveryService::socketError()
{
    qWarning() << "socket error:" << mSocket.errorString();
}

void DiscoveryService::onReadyRead()
{
    while (mSocket.hasPendingDatagrams())
    {
        datagramReceived();
    }
}

void DiscoveryService::datagramReceived()
{
    QNetworkDatagram datagram = mSocket.receiveDatagram();

    emit peerDiscovered(datagram.senderAddress(), datagram.interfaceIndex());
    qDebug() << QDateTime::currentDateTime().toString() << "Discovery from " << datagram.senderAddress().toString() << " if " << datagram.interfaceIndex();
}
