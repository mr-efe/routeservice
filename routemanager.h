#ifndef ROUTEMANAGER_H
#define ROUTEMANAGER_H

#include <QObject>
#include <QHostAddress>
#include <QSet>
#include <winsock2.h>
#include <iphlpapi.h>

class RouteManager : public QObject
{
    Q_OBJECT
public:
    explicit RouteManager(QObject* parent = 0);
    virtual ~RouteManager();
public slots:
    void onPeerDiscovered(const QHostAddress& ip, uint networkInterfaceIndex);
    void onInterfaceChanged();
private:
    bool addRoute(DWORD destAddr, DWORD interfaceIndex);
    MIB_IPFORWARDROW findRoute(DWORD destAddr);
    void deleteAllRoutes();
    void deleteRoute(DWORD  destAddr);
    QString getWindowsErrorMessage(DWORD errorCode);
private:
    QSet<DWORD> mRoutes;
};

#endif // ROUTEMANAGER_H
