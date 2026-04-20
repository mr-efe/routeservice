#include "routemanager.h"
#include <QDebug>
#include <QNetworkInterface>

RouteManager::RouteManager(QObject* parent)
    : QObject(parent) {
}

RouteManager::~RouteManager()
{
    foreach(DWORD destAddr, mRoutes) {
        deleteRoute(destAddr);
    }
}

void RouteManager::onPeerDiscovered(const QHostAddress &ip, uint networkInterfaceIndex)
{
    if(QNetworkInterface::allAddresses().contains(ip) ) {
        //don't add route to ourself
        return;
    }

    //check if route exist
    DWORD destAddr = inet_addr(ip.toString().toStdString().c_str());
    bool addNewRoute = false;
    MIB_IPFORWARDROW route = findRoute(destAddr);
    if(route.dwForwardDest == destAddr) {
        if(route.dwForwardIfIndex != networkInterfaceIndex){
            //interface has changed
            qInfo() << "interface to " << ip.toString() << " has changed from "
                    << QNetworkInterface::interfaceFromIndex(route.dwForwardIfIndex).humanReadableName() << " to " << QNetworkInterface::interfaceFromIndex(networkInterfaceIndex).humanReadableName() << ", deleting old route.";
            deleteRoute(route.dwForwardDest);
            addNewRoute = true;
        }
    } else {
        addNewRoute = true;
    }

    //add new route
    if(addNewRoute) {
        addRoute(destAddr, networkInterfaceIndex);
    }

    mRoutes.insert(destAddr);
}

void RouteManager::onInterfaceChanged()
{
    deleteAllRoutes();
}

bool RouteManager::addRoute(DWORD destAddr, DWORD interfaceIndex)
{
    MIB_IPFORWARDROW route;
    ZeroMemory(&route, sizeof(MIB_IPFORWARDROW));
    route.dwForwardDest = destAddr;
    route.dwForwardMask = inet_addr("255.255.255.255");
    route.dwForwardNextHop = 0;
    route.dwForwardIfIndex = interfaceIndex;
    route.dwForwardType = MIB_IPROUTE_TYPE_DIRECT;
    route.dwForwardProto = MIB_IPPROTO_NETMGMT;
    route.dwForwardMetric1 = 100;
    route.dwForwardMetric2 = 0;
    route.dwForwardMetric3 = 0;
    route.dwForwardMetric4 = 0;
    route.dwForwardMetric5 = 0;

    DWORD result = CreateIpForwardEntry(&route);

    if (result != NO_ERROR) {
        qWarning() << "error on creating route to " << QHostAddress(htonl(destAddr)).toString() << ": " << getWindowsErrorMessage(result);
    } else {
        qInfo() << "added route to " << QHostAddress(htonl(destAddr)).toString()  << " on interface " << QNetworkInterface::interfaceFromIndex(interfaceIndex).humanReadableName();
    }
    return true;
}

void RouteManager::deleteRoute(DWORD destAddr)
{
    mRoutes.remove(destAddr);

    DWORD result = NO_ERROR;
    MIB_IPFORWARDROW route = findRoute(destAddr);
    if(route.dwForwardDest != 0) {
        result = DeleteIpForwardEntry(&route);
    }

    if(result == NO_ERROR) {
        qInfo() << "deleted route to " << QHostAddress(htonl(destAddr)).toString();
    } else {
        qWarning() << "error on deleting route to " << QHostAddress(htonl(destAddr)).toString() << ": " << getWindowsErrorMessage(result);
    }
}

MIB_IPFORWARDROW RouteManager::findRoute(DWORD destAddr)
{
    MIB_IPFORWARDROW route;
    PMIB_IPFORWARDTABLE pIpForwardTable;
    DWORD dwSize = 0;
    DWORD maskAddr = inet_addr("255.255.255.255");

    pIpForwardTable = (MIB_IPFORWARDTABLE*)malloc(sizeof(MIB_IPFORWARDTABLE));

    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
        free(pIpForwardTable);
        pIpForwardTable = (MIB_IPFORWARDTABLE*)malloc(dwSize);
    }

    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pIpForwardTable->dwNumEntries; i++) {
            MIB_IPFORWARDROW* pRow = &pIpForwardTable->table[i];

            if (pRow->dwForwardDest == destAddr &&
                pRow->dwForwardMask == maskAddr) {
                route = *pRow;
                break;
            }
        }
    }

    if (pIpForwardTable)
        free(pIpForwardTable);

    return route;
}

void RouteManager::deleteAllRoutes()
{
    foreach(DWORD destAddr, mRoutes) {
        deleteRoute(destAddr);
    }
    mRoutes.clear();
}

QString RouteManager::getWindowsErrorMessage(DWORD errorCode)
{
    LPWSTR messageBuffer = nullptr;

    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer,
        0,
        NULL
    );

    QString message;
    if (size > 0 && messageBuffer) {
        message = QString::fromWCharArray(messageBuffer, size).trimmed();
        LocalFree(messageBuffer);
    } else {
        message = QString("Unknown error code: %1").arg(errorCode);
    }

    return message;
}
