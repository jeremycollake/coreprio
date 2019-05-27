// service installation code derived from an MSDN sample
#include "pch.h"
#include <stdio.h> 
#include <windows.h> 
#include "ServiceInstaller.h" 

void InstallService(const WCHAR * pszServiceName,
	const WCHAR* pszDisplayName,
	DWORD dwStartType,
	const WCHAR* pszDependencies,
	PWSTR pszAccount,
	PWSTR pszPassword)
{
	wchar_t szPath[MAX_PATH];
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;

	if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)) == 0)
	{
		wprintf(L"GetModuleFileName failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT |
		SC_MANAGER_CREATE_SERVICE);
	if (schSCManager == NULL)
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}
	
	schService = CreateService(
		schSCManager,                   // SCManager database 
		pszServiceName,                 // Name of service 
		pszDisplayName,                 // Name to display 
		SERVICE_QUERY_STATUS,           // Desired access 
		SERVICE_WIN32_OWN_PROCESS,      // Service type 
		dwStartType,                    // Service start type 
		SERVICE_ERROR_NORMAL,           // Error control type 
		szPath,                         // Service's binary 
		NULL,                           // No load ordering group 
		NULL,                           // No tag identifier 
		pszDependencies,                // Dependencies 
		pszAccount,                     // Service running account 
		pszPassword                     // Password of the account 
	);
	if (schService == NULL)
	{
		wprintf(L"CreateService failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	wprintf(L"%s is installed.\n", pszServiceName);

Cleanup:
	// Centralized cleanup for all allocated resources. 
	if (schSCManager)
	{
		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
	if (schService)
	{
		CloseServiceHandle(schService);
		schService = NULL;
	}
}

void UninstallService(const WCHAR * pszServiceName)
{
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;
	SERVICE_STATUS ssSvcStatus = {};

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager == NULL)
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	schService = OpenService(schSCManager, pszServiceName, SERVICE_STOP |
		SERVICE_QUERY_STATUS | DELETE);
	if (schService == NULL)
	{
		wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus))
	{
		wprintf(L"Stopping %s.", pszServiceName);
		Sleep(1000);

		while (QueryServiceStatus(schService, &ssSvcStatus))
		{
			if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING)
			{
				wprintf(L".");
				Sleep(1000);
			}
			else break;
		}

		if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED)
		{
			wprintf(L"\n%s is stopped.\n", pszServiceName);
		}
		else
		{
			wprintf(L"\n%s failed to stop.\n", pszServiceName);
		}
	}

	if (!DeleteService(schService))
	{
		wprintf(L"DeleteService failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	wprintf(L"%s is removed.\n", pszServiceName);

Cleanup:
	if (schSCManager)
	{
		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
	if (schService)
	{
		CloseServiceHandle(schService);
		schService = NULL;
	}
}