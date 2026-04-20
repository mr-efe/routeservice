#include "macdservice.h"
#include <QTcpSocket>
#include <QNetworkInterface>
#include <QDateTime>
#include <QByteArray>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

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
    connect(&mTcpPollTimer, &QTimer::timeout, this, &MacDService::onTcpPollTimer);
    mSendMacDTimer.setInterval(60000); //every minute
    mSendMacDTimer.start();
    mTcpPollTimer.setInterval(500);

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
            if(!mTcpPollTimer.isActive()) {
                mTcpPollTimer.start();
            }
        } else {
            mTcpPollTimer.stop();
            mKnownTcpConnections.clear();
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

void MacDService::onTcpPollTimer()
{
    if(mTcpServer.isListening() || mTcpServer.listen(QHostAddress::AnyIPv4, MACD_TCP_PORT)) {
        mTcpPollTimer.stop();
        mKnownTcpConnections.clear();
        return;
    }

    checkViaExtendedTcpTable();
}

void MacDService::checkViaExtendedTcpTable()
{
    DWORD tcpTableSize = 0;
    DWORD result = GetExtendedTcpTable(nullptr, &tcpTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if(result != ERROR_INSUFFICIENT_BUFFER && result != NO_ERROR) {
        qWarning() << "GetExtendedTcpTable failed while querying buffer size:" << result;
        return;
    }

    QByteArray tcpTableBuffer;
    tcpTableBuffer.resize(static_cast<int>(tcpTableSize));
    PMIB_TCPTABLE_OWNER_PID tcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(tcpTableBuffer.data());

    result = GetExtendedTcpTable(tcpTable, &tcpTableSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if(result != NO_ERROR) {
        qWarning() << "GetExtendedTcpTable failed:" << result;
        return;
    }

    QSet<QString> currentConnections;

    for(DWORD i = 0; i < tcpTable->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID& row = tcpTable->table[i];
        const quint16 localPort = ntohs(static_cast<quint16>(row.dwLocalPort & 0xFFFF));
        if(localPort != MACD_TCP_PORT || row.dwState != MIB_TCP_STATE_ESTAB) {
            continue;
        }

        IN_ADDR remoteAddr {};
        remoteAddr.S_addr = row.dwRemoteAddr;
        IN_ADDR localAddr {};
        localAddr.S_addr = row.dwLocalAddr;

        char remoteIpBuffer[INET_ADDRSTRLEN] = {};
        char localIpBuffer[INET_ADDRSTRLEN] = {};
        if(!inet_ntop(AF_INET, &remoteAddr, remoteIpBuffer, sizeof(remoteIpBuffer))
                || !inet_ntop(AF_INET, &localAddr, localIpBuffer, sizeof(localIpBuffer))) {
            continue;
        }

        const QString remoteIp = QString::fromLatin1(remoteIpBuffer);
        const QHostAddress remoteHostAddress(remoteIp);
        const QHostAddress localHostAddress(QString::fromLatin1(localIpBuffer));
        currentConnections.insert(remoteIp);

        if(mKnownTcpConnections.contains(remoteIp)) {
            continue;
        }

        for(const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
            bool localAddressMatchesInterface = false;
            for(const QNetworkAddressEntry& addrEntry : interface.addressEntries()) {
                if(addrEntry.ip() == localHostAddress) {
                    qDebug() << QDateTime::currentDateTime().toString() << "MacD Answer from " << remoteHostAddress.toString() << " if " << interface.index();
                    emit peerDiscovered(remoteHostAddress, interface.index());
                    localAddressMatchesInterface = true;
                    break;
                }
            }

            if(localAddressMatchesInterface) {
                break;
            }
        }
    }

    mKnownTcpConnections = currentConnections;
}
