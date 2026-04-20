#include "macdservice.h"
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QDateTime>

MacDService::MacDService(QObject *parent)
    : QObject{parent}
{
    if(!mUdpSocket.bind(QHostAddress::AnyIPv4, MACD_UDP_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCritical() << "could not bind to address!";
    }

    mUdpSocket.setSocketOption(QAbstractSocket::MulticastTtlOption, 255);
    if(!mUdpSocket.joinMulticastGroup(MULTICAST_ADDR)) {
        qCritical() << "Failed to join multicast group!";
    }

    connect(&mUdpSocket, SIGNAL(interfaceChanged()), this, SLOT(onInterfaceChanged()));
    connect(&mSendMacDTimer, SIGNAL(timeout()), this ,SLOT(sendMacD()));
    mSendMacDTimer.setInterval(60000); //every minute
    mSendMacDTimer.start();

    sendMacD();
}

MacDService::~MacDService()
{
    mTcpServer.close();
}

void MacDService::onInterfaceChanged()
{
    mTcpServer.close();
    sendMacD();
}

void MacDService::sendMacD()
{
    if(!mTcpServer.isListening()) {
        connect(&mTcpServer, SIGNAL(newConnection()), this, SLOT(onNewTcpConnection()), Qt::UniqueConnection);
        mTcpServer.listen(QHostAddress::AnyIPv4, MACD_TCP_PORT);
        if(!mTcpServer.isListening()) {
            qWarning() << "TcpServer not listening: " << mTcpServer.errorString();
        }
    }

    if(mUdpSocket.writeDatagram(QString("REPORT_MAC_ADDRESS%1").arg(rand()).toLocal8Bit(), MULTICAST_ADDR, MACD_UDP_PORT)) {
        qDebug() << "Sending MacD Report Message";
    } else {
        qWarning() << "Sending MacD REPORT_MAC_ADDRESS command failed: " << mUdpSocket.errorString();
    }
}

void MacDService::onNewTcpConnection()
{
    QTcpSocket* socket = mTcpServer.nextPendingConnection();

    foreach(QNetworkInterface interface, QNetworkInterface::allInterfaces()) {
        foreach(QNetworkAddressEntry addrEntry, interface.addressEntries()) {
            if(addrEntry.ip() == socket->localAddress()) {
                qDebug() << QDateTime::currentDateTime().toString() << "MacD Answer from " << socket->peerAddress().toString() << " if " << interface.index();
                emit peerDiscovered(socket->peerAddress(), interface.index());
                socket->deleteLater();
                return;
            }
        }
    }
    socket->deleteLater();
}
