#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QSharedPointer>
#include <QFile>
#include <QDir>
#include <QProcess>

#include "Logging.h"
#include "discoveryservice.h"
#include "macdservice.h"
#include "routemanager.h"

TCHAR serviceName[] = TEXT("EFE Route Service");
SERVICE_STATUS serviceStatus;
SERVICE_STATUS_HANDLE serviceStatusHandle = 0;

void run(int argc, char* argv[] )
{
    QCoreApplication a(argc,argv);

    //init logging
    QDir loggingDir("C:/ProgramData/EFE GmbH/route_service/logs");
    if(!loggingDir.exists()) {
        if(!loggingDir.mkpath(loggingDir.absolutePath())) {
            loggingDir = QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).absoluteFilePath("LOGGING_DIR_NAME"));
        }
    }
    Logging::setLogfileDirPath(loggingDir.absolutePath());
    Logging::setLogfileEnding(".log");
    Logging::setTypes(Logging::Type::All);
    Logging::init();

    //start services
    DiscoveryService discoveryService;
    MacDService macDService;
    RouteManager routeManager;
    QObject::connect(&discoveryService, &DiscoveryService::peerDiscovered, &routeManager, &RouteManager::onPeerDiscovered);
    QObject::connect(&macDService, &MacDService::peerDiscovered, &routeManager, &RouteManager::onPeerDiscovered);
    a.exec();
}
#ifdef QT_DEBUG

int main(int argc, char* argv[])
{
    run(argc,argv);
}
#else

void WINAPI ServiceControlHandler( DWORD controlCode )
{
    switch ( controlCode )
    {
        case SERVICE_CONTROL_INTERROGATE:
            break;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus( serviceStatusHandle, &serviceStatus );

            qApp->quit();

            serviceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus( serviceStatusHandle, &serviceStatus );
            return;

        case SERVICE_CONTROL_PAUSE:
            break;

        case SERVICE_CONTROL_CONTINUE:
            break;

        default:
            if ( controlCode >= 128 && controlCode <= 255 )
                // user defined control code
                break;
            else
                // unrecognised control code
                break;
    }

    SetServiceStatus( serviceStatusHandle, &serviceStatus );
}

void WINAPI ServiceMain(DWORD argc, TCHAR* argv[])
{
    Q_UNUSED(argc)
    Q_UNUSED(argv)

    // initialise service status
    serviceStatus.dwServiceType = SERVICE_WIN32;
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwControlsAccepted = 0;
    serviceStatus.dwWin32ExitCode = NO_ERROR;
    serviceStatus.dwServiceSpecificExitCode = NO_ERROR;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;

    serviceStatusHandle = RegisterServiceCtrlHandler( serviceName, ServiceControlHandler );

    if ( serviceStatusHandle )
    {
        // service is starting
        serviceStatus.dwCurrentState = SERVICE_START_PENDING;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );

        // running
        serviceStatus.dwControlsAccepted |= (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );

        run(0, NULL);

        // service was stopped
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );

        // service is now stopped
        serviceStatus.dwControlsAccepted |= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus( serviceStatusHandle, &serviceStatus );
    } else {
        qWarning() << "No Service Hanlde";
    }
}

void RunService()
{
    qInfo() << "Running Service...";
    SERVICE_TABLE_ENTRY serviceTable[] =
    {
        { serviceName, ServiceMain },
        { 0, 0 }
    };

    StartServiceCtrlDispatcher( serviceTable );
}

void InstallService()
{
    qInfo() << "Installing Service...";

    SC_HANDLE serviceControlManager = OpenSCManager( 0, 0, SC_MANAGER_CREATE_SERVICE );

    if ( serviceControlManager )
    {
        TCHAR path[ _MAX_PATH + 1 ];
        if ( GetModuleFileName( 0, path, sizeof(path)/sizeof(path[0]) ) > 0 )
        {
            SC_HANDLE service = CreateService( serviceControlManager,
                            serviceName, serviceName,
                            SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                            SERVICE_AUTO_START, SERVICE_ERROR_IGNORE, path,
                            0, 0, 0, 0, 0 );
            if ( service ) {
                CloseServiceHandle( service );

                // Add Firewall roules
                system(QString("netsh advfirewall firewall add rule name=\"EFE Route Service\" "
                                            "dir=in action=allow program=\"%1\" enable=yes")
                                            .arg(QString::fromWCharArray(path)).toStdString().c_str());

                system(QString("netsh advfirewall firewall add rule name=\"EFE Route Service\" "
                                            "dir=out action=allow program=\"%1\" enable=yes")
                                            .arg(QString::fromWCharArray(path)).toStdString().c_str());
            }
        }

        CloseServiceHandle( serviceControlManager );
    }
}

void UninstallService()
{
    qInfo() << "Uninstalling Service...";
    SC_HANDLE serviceControlManager = OpenSCManager( 0, 0, SC_MANAGER_CONNECT );

    if ( serviceControlManager )
    {
        SC_HANDLE service = OpenService( serviceControlManager,
            serviceName, SERVICE_QUERY_STATUS | DELETE );
        if ( service )
        {
            SERVICE_STATUS status;
            if ( QueryServiceStatus( service, &status ) )
            {
                if ( status.dwCurrentState == SERVICE_STOPPED ) {
                    DeleteService( service );

                    // Remove Firewall rules
                    system(QString("netsh advfirewall firewall delete rule name=\"EFE Route Service\" ").toStdString().c_str());
                }
            }

            CloseServiceHandle( service );
        }

        CloseServiceHandle( serviceControlManager );
    }
}

int main(int argc, char* argv[])
{
    if ( argc > 1 && (QString(argv[1]) == "install"))
    {
        InstallService();
    }
    else if ( argc > 1 && (QString(argv[1]) == "uninstall"))
    {
        UninstallService();
    }
    else
    {
        RunService();
    }

    return EXIT_SUCCESS;
}

#endif
