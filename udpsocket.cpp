#include "UdpSocket.h"

#include <QNetworkInterface>
#include <QDataStream>
#include <QLocalSocket>
#include <QTimerEvent>
#include <QDebug>

UdpSocket::UdpSocket(QObject *parent) : QUdpSocket(parent)
{
    checkAddresses();

    mTimer=startTimer(5000);
}

UdpSocket::~UdpSocket()
{
}

bool UdpSocket::joinMulticastGroup(const QHostAddress &groupAddress)
{
    if(!mMulticastAddrs.contains(groupAddress))
       mMulticastAddrs.append(groupAddress);
    bool ret=true;
    int icnt= 0;
    foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        if(iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
                iface.flags().testFlag(QNetworkInterface::IsRunning) &&
                iface.flags().testFlag(QNetworkInterface::IsUp) )
        {
            QUdpSocket::setMulticastInterface(iface);
            if(!QUdpSocket::joinMulticastGroup(groupAddress,iface))
            {
                qWarning()<<"Cannot join group " << groupAddress.toString() << " on " << iface.humanReadableName();
                ret=false;
            }
            else
                icnt++;
        }
    }
    if(!icnt)
    {
        qWarning()<<"woops, looks like you're not online !";
        ret=false;
        foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
        {
                QUdpSocket::setMulticastInterface(iface);

                if(QUdpSocket::joinMulticastGroup(groupAddress,iface))
                ret=true;
        }
    }
    return ret;
}

bool UdpSocket::leaveMulticastGroup(const QHostAddress &groupAddress)
{
    if(!mMulticastAddrs.contains(groupAddress))
        return false;

    mMulticastAddrs.removeAll(groupAddress);
    bool ret=true;
    int icnt = 0;
    foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        if(iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
                iface.flags().testFlag(QNetworkInterface::IsRunning) &&
                iface.flags().testFlag(QNetworkInterface::IsUp) )
        {
            QUdpSocket::setMulticastInterface(iface);
            if(!QUdpSocket::leaveMulticastGroup(groupAddress,iface))
            {
                qWarning() << "Cannot leave group" << groupAddress.toString() << " on "<<iface.humanReadableName();
                ret=false;
            }
            else {
                icnt++;
            }
        }
    }
    if(!icnt)
    {
        qWarning()<<"woops, looks like you're not online !";
        if(!QUdpSocket::leaveMulticastGroup(groupAddress))
        {
            qWarning()<<"Cannot leave group"<<groupAddress.toString();
            ret=false;
        }
    }
    return ret;

}

void UdpSocket::timerEvent(QTimerEvent *e)
{
    if(e->timerId() == mTimer) {
        checkAddresses();
    }
    QUdpSocket::timerEvent(e);
}

void UdpSocket::checkAddresses()
{
    quint32 chs=0;
    foreach ( QNetworkInterface interface, QNetworkInterface::allInterfaces())
    {
        if (interface.flags().testFlag(QNetworkInterface::CanMulticast) && !interface.flags().testFlag( QNetworkInterface::IsLoopBack) &&
                interface.flags().testFlag(QNetworkInterface::IsRunning) && interface.flags().testFlag( QNetworkInterface::IsUp) ){
            foreach (QNetworkAddressEntry addr, interface.addressEntries()) {
                    chs += addr.ip().toIPv4Address();
             }
        }
    }
    if(chs != mChecksum)
    {
        if (mChecksum)
        {
            foreach (QHostAddress addr, mMulticastAddrs) {
                leaveMulticastGroup(addr); //TODO Check if leave is really needed there
                joinMulticastGroup(addr);
            }

            emit interfaceChanged();
        }

        mChecksum  = chs;
    }
}

qint64 UdpSocket::writeDatagram(const char* data,int size, const QHostAddress &host, quint16 port)
{
    qint64 ret=-1;
    if (!((host.toIPv4Address() & 0xF0000000) == 0xE0000000)) //taken from Qt5.6 sources ( isMulticast )
    {
         return QUdpSocket::writeDatagram(data,size,host,port);
    }
    int icnt = 0;
    foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        if(iface.flags().testFlag(QNetworkInterface::CanMulticast) &&
                iface.flags().testFlag(QNetworkInterface::IsRunning) &&
                iface.flags().testFlag(QNetworkInterface::IsUp) )
        {
            QUdpSocket::setMulticastInterface(iface);
            ret=QUdpSocket::writeDatagram(data,size,host,port);

            if( ret != 0 )
                icnt++;

        }
    }
    if(!icnt) {
        qWarning()<<"Warning ! no working network interface for multicast found ! Try to send anyway";
        ret=QUdpSocket::writeDatagram(data,size,host,port);
    }
    return ret;
}
