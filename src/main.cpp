//
//  main.cpp
//  PromoCalculator
//
//  Created by Andrea Giovacchini on 04/01/14.
//  Copyright (c) 2014 Andrea Giovacchini. All rights reserved.
//
#include "base_types.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <csignal>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "server.hpp"

#include "base_system.h"

#if defined _WIN32
	#include <windows.h>
	#include <tchar.h>
	#include <strsafe.h>
	#pragma comment(lib, "advapi32.lib")

	#define SVCNAME TEXT("PromoCalculator")
	#define SVC_ERROR                        ((DWORD)0xC0020001L)
	SERVICE_STATUS          gSvcStatus; 
	SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
	HANDLE                  ghSvcStopEvent = NULL;

	TCHAR szCommand[10];
	TCHAR szSvcName[80];

	SC_HANDLE schSCManager;
	SC_HANDLE schService;

	VOID SvcInstall(void);

	VOID WINAPI SvcCtrlHandler( DWORD ); 
	VOID WINAPI SvcMain( DWORD, LPTSTR * ); 

	VOID ReportSvcStatus( DWORD, DWORD, DWORD );
	VOID SvcInit( DWORD, LPTSTR * ); 
	VOID SvcReportEvent( LPTSTR );
	void WinMainNoSvc();
	void WinMainSvc();
#endif 

using namespace std;

string main_path ;
string ini_file_name ;
src::severity_logger_mt< boost::log::trivial::severity_level > my_logger_main;

void init(const string& main_path, const string& ini_file_name)
{
	const std::string base_log_name = "logs/Promocalculator.log";
	const std::string prev_log_suffix = ".prev";
	std::cout << main_path + base_log_name;
	fileDelete(main_path + base_log_name + prev_log_suffix);
	fileMove(main_path + base_log_name, main_path + base_log_name + prev_log_suffix);

    logging::add_file_log
    (
     keywords::file_name = main_path + base_log_name,
     keywords::auto_flush = true, 
     // This makes the sink to write log records that look like this:
     // YYYY-MM-DD HH:MI:SS: <normal> A normal severity message
     // YYYY-MM-DD HH:MI:SS: <error> An error severity message
     keywords::format =
     (
      expr::stream
      << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
      << ": <" << logging::trivial::severity
      << "> " << expr::smessage
      )
     );

#if !defined(_WIN32)

	// Block all signals for background thread.
    sigset_t new_mask;
    sigfillset(&new_mask);
    sigset_t old_mask;
    pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
#endif
		

    logging::add_common_attributes();
	src::severity_logger_mt< boost::log::trivial::severity_level > my_logger_ma;
}

#if !defined(_WIN32)
	#include <pthread.h>
	#include <signal.h>

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: promoCalculator rootPath ini_file_name" << std::endl ;
            return 1;
        }
        
        main_path = argv[1];
        ini_file_name = argv[2] ;
        
        init(main_path, ini_file_name);
        
        sigset_t new_mask;
        sigfillset(&new_mask);
        sigset_t old_mask;
        pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);
        
        logging::add_common_attributes();

        BaseSystem *bs = nullptr;
        bs = new BaseSystem(main_path, ini_file_name);
        
        // Run server in background thread.
     
        auto num_threads = boost::lexical_cast<std::size_t>(bs->getConfigValue("WebThreads").c_str());
		http::server3::server s(bs->getConfigValue("WebAddress").c_str(), bs->getConfigValue("WebPort"), main_path + "/DocRoot/", num_threads, bs);
        boost::thread t([ObjectPtr = &s] { ObjectPtr->run(); });
        
        BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - " << "Started http server" ;
        
        
        std::string pidFileName = main_path + "/promoCalculator.pid" ;
        std::ofstream pidFile ;
        pidFile.open( pidFileName );
        pidFile << getpid() << std::endl ;
        pidFile.close() ;
        
        std::cout << "Started with pid: " << getpid() << std::endl ;
        
        // Wait for signal indicating time to shut down.
        sigset_t wait_mask;
        sigemptyset(&wait_mask);
        sigaddset(&wait_mask, SIGINT);
        sigaddset(&wait_mask, SIGQUIT);
        sigaddset(&wait_mask, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &wait_mask, nullptr);
        int sig = 0;
        sigwait(&wait_mask, &sig);
		BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - " << "Received stop request";
        s.stop();
        t.join();
		BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - " << "Finished processing stop request";
        std::cout << "Process exit " << fileDelete(pidFileName) << std::endl ;
    }
    catch (std::exception& e)
    {
        std::cerr << "exception: " << e.what() << "\n";
    }
    
    return 0;
}

#endif // !defined(_WIN32)

/*
#if !defined _WIN32
int main(int argc, const char * argv[])
{
    main_path = argv[1] ;

	init(main_path);
    logging::add_common_attributes();

    BaseSystem *bs = nullptr;
	bs = new BaseSystem(main_path, ini_file_name);

	return 0;
}
#endif
*/

#if defined _WIN32

short keepRunning = 1;

BOOL CtrlHandler(DWORD fdwCtrlType)
{
	printf("Received stop event\n\n");
	keepRunning = 0;
	return TRUE;
}

int __cdecl _tmain(int argc, TCHAR *argv[]) 
{ 
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.
    /*if( argc != 1 )
    {
        printf("ERROR: Incorrect number of arguments\n\n");
        DisplayUsage();
        return 0;
    }*/
    
    /*Visual Studio: Works only if in "Project settings | Configuration properties | General"
     the setting "Character Set" is "Use Multi-Byte Character Set"
     http://stackoverflow.com/questions/6006319/converting-tchar-to-string-in-c
     */
	main_path = argv[1] ;
	ini_file_name = argv[2];

	if (strcmp(argv[3], "SVZ") == 0) {
		WinMainSvc();
	} else {
		WinMainNoSvc();
	}

} 

void WinMainNoSvc() {
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
	std::cout << main_path << endl;
	init(main_path, ini_file_name);
	BaseSystem *bs = nullptr;
	bs = new BaseSystem(main_path, ini_file_name);

	// Run server in background thread.
	std::size_t num_threads = boost::lexical_cast<std::size_t>(bs->getConfigValue("WebThreads").c_str());

	http::server3::server s(bs->getConfigValue("WebAddress").c_str(), bs->getConfigValue("WebPort").c_str(), main_path + "/DocRoot/", num_threads, bs);
	boost::thread t(boost::bind(&http::server3::server::run, &s));

	BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - Started http server";
	// ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);


	std::string pidFileName = main_path + "/promoCalculator.pid";
	std::ofstream pidFile;
	pidFile.open(pidFileName);
	pidFile << _getpid() << std::endl;
	pidFile.close();

	BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - Service running with pid: " << _getpid();
	
	while (keepRunning == 1)
	{
	}
	// Check whether to stop the service.

		
	BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - Received stop request";

	std::cout << "Process exit " << fileDelete(pidFileName) << std::endl;

	ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);

	BOOST_LOG_SEV(my_logger_main, lt::info) << "- MA - Finished processing stop request";

	keepRunning = 2;

	return;
	
}

void WinMainSvc() {
	//    if( lstrcmpi( argv[1], TEXT("install")) == 0 )
	//    {
	SvcInstall();
	//        return 0;
	//    }

	// TO_DO: Add any additional services for the process to this table.
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
		{ NULL, NULL }
	};

	// This call returns when the service has stopped. 
	// The process should simply terminate when the call returns.

	if (!StartServiceCtrlDispatcher(DispatchTable))
	{
		SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
	}
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if( !GetModuleFileName( NULL, szPath, MAX_PATH ) )
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database. 
 
    schSCManager = OpenSCManager( 
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
 
    if (NULL == schSCManager) 
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Create the service

    schService = CreateService( 
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 
 
    if (schService == NULL) 
    {
        printf("CreateService failed (%d)\n", GetLastError()); 
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service installed successfully\n"); 

    CloseServiceHandle(schService); 
    CloseServiceHandle(schSCManager);
}


//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler( 
        SVCNAME, 
        SvcCtrlHandler);

    if( !gSvcStatusHandle )
    { 
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler")); 
        return; 
    } 

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    gSvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM

    ReportSvcStatus( SERVICE_START_PENDING, NO_ERROR, 3000 );

    // Perform service-specific initialization and work.

    SvcInit( dwArgc, lpszArgv );
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit( DWORD dwArgc, LPTSTR *lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.

    ghSvcStopEvent = CreateEvent(
                         NULL,    // default security attributes
                         TRUE,    // manual reset event
                         FALSE,   // not signaled
                         NULL);   // no name

    if ( ghSvcStopEvent == NULL)
    {
        ReportSvcStatus( SERVICE_STOPPED, NO_ERROR, 0 );
        return;
    }

    // Report running status when initialization is complete.
    
	WinMainSvc();
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus( DWORD dwCurrentState,
                      DWORD dwWin32ExitCode,
                      DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ( (dwCurrentState == SERVICE_RUNNING) ||
           (dwCurrentState == SERVICE_STOPPED) )
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus( gSvcStatusHandle, &gSvcStatus );
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler( DWORD dwCtrl )
{
   // Handle the requested control code. 

   switch(dwCtrl) 
   {  
      case SERVICE_CONTROL_STOP: 
         ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

         // Signal the service to stop.

         SetEvent(ghSvcStopEvent);
         ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
         
         return;
 
      case SERVICE_CONTROL_INTERROGATE: 
         break; 
 
      default: 
         break;
   } 
   
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction) 
{ 
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if( NULL != hEventSource )
    {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
                    EVENTLOG_ERROR_TYPE, // event type
                    0,                   // event category
                    SVC_ERROR,           // event identifier
                    NULL,                // no security identifier
                    2,                   // size of lpszStrings array
                    0,                   // no binary data
                    lpszStrings,         // array of strings
                    NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}

#endif
