/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*
* ! LEGACY CODE WARNING (15+ years)
*
*/
#include "stdafx.h"
#include "ServiceManager.h"

bool ServiceManager::EnsureServiceIsEnabled(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is enabled in its boot config, if not then enable it
	DWORD dwBytesNeeded = 0;
	QueryServiceConfig(scTargetService, NULL, 0, &dwBytesNeeded);
	if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	QUERY_SERVICE_CONFIG *pServiceConfig = reinterpret_cast<QUERY_SERVICE_CONFIG *>(new BYTE[dwBytesNeeded]);
	memset(pServiceConfig, 0, dwBytesNeeded);
	if (!QueryServiceConfig(scTargetService, pServiceConfig, dwBytesNeeded, &dwBytesNeeded))
	{
		delete pServiceConfig;
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	if (pServiceConfig->dwStartType == SERVICE_DISABLED)
	{		
		pServiceConfig->dwStartType = SERVICE_DEMAND_START;		
		if (!ChangeServiceConfig(scTargetService,
			pServiceConfig->dwServiceType,
			pServiceConfig->dwStartType,
			pServiceConfig->dwErrorControl,
			pServiceConfig->lpBinaryPathName,
			pServiceConfig->lpLoadOrderGroup,
			&pServiceConfig->dwTagId,
			pServiceConfig->lpDependencies,
			pServiceConfig->lpServiceStartName,
			NULL,
			pServiceConfig->lpDisplayName))
		{
			delete pServiceConfig;
			CloseServiceHandle(scTargetService);
			CloseServiceHandle(scServiceManager);
			return false;
		}
		NotifyBootConfigStatus(TRUE);
	}
	delete pServiceConfig;
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return true;
}

bool ServiceManager::DoesServiceExist(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_READ | SC_MANAGER_CONNECT);
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_INTERROGATE | SERVICE_QUERY_STATUS);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}		
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return true;
}

bool ServiceManager::Stop(const WCHAR *ptszServiceName, const unsigned int nMaxWaitMs)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is started already or not .. try to start if so
	SERVICE_STATUS serviceStatus;
	memset(&serviceStatus, 0, sizeof(SERVICE_STATUS));
	if (!QueryServiceStatus(scTargetService, &serviceStatus))
	{
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	if (serviceStatus.dwCurrentState != SERVICE_STOPPED && serviceStatus.dwCurrentState != SERVICE_STOP_PENDING)
	{
		if (!ControlService(scTargetService, SERVICE_CONTROL_STOP, &serviceStatus))
		{
			CloseServiceHandle(scTargetService);
			CloseServiceHandle(scServiceManager);
			return false;
		}
	}	
	bool bR = nMaxWaitMs ? false : true; // return true (success) if we aren't waiting
	if (nMaxWaitMs)
	{
		// service handles are NOT waitable objects
		// MSDN says to use polling, so we will
		// there a time to wait hint provided in the service config, but unreliable		
		for (unsigned int nI = 0; nI < nMaxWaitMs/recheckStatusIntervalMs; nI++)
		{			
			if (IsServiceStopped(ptszServiceName))
			{
				bR = true;
				break;
			}
			Sleep(recheckStatusIntervalMs);
		}
	}	
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return bR;
}

bool ServiceManager::Start(const WCHAR *ptszServiceName, const unsigned int nMaxWaitMs)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS);
	if (!scTargetService)
	{		
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is started already or not .. try to start if so
	SERVICE_STATUS serviceStatus;
	memset(&serviceStatus, 0, sizeof(SERVICE_STATUS));
	if (!QueryServiceStatus(scTargetService, &serviceStatus))
	{		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	if (serviceStatus.dwCurrentState != SERVICE_RUNNING)
	{
		if (serviceStatus.dwCurrentState != SERVICE_START_PENDING)
		{
			if (!StartService(scTargetService, 0, NULL))
			{				
				CloseServiceHandle(scTargetService);
				CloseServiceHandle(scServiceManager);
				return false;
			}
		}
	}
	bool bR = nMaxWaitMs ? false : true; // return true (success) if we aren't waiting
	if (nMaxWaitMs)
	{
		DEBUG_PRINT(L"ServiceManager - waiting for start to complete");
		// service handles are NOT waitable objects
		// MSDN says to use polling, so we will
		// there a time to wait hint provided in the service config, but unreliable
		for (unsigned int nI = 0; nI < nMaxWaitMs/recheckStatusIntervalMs; nI++)
		{			
			if (IsServiceStarted(ptszServiceName))
			{
				bR = true;
				break;
			}
			Sleep(recheckStatusIntervalMs);
		}
	}	
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return true;
}

bool ServiceManager::IsServiceDisabled(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (!scServiceManager)
	{		
		return false;
	}	
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, STANDARD_RIGHTS_READ | SERVICE_QUERY_CONFIG);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}	
	DWORD dwBytesNeeded = 0;
	QueryServiceConfig(scTargetService, NULL, 0, &dwBytesNeeded);
	if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}	
	QUERY_SERVICE_CONFIG *pServiceConfig = reinterpret_cast<QUERY_SERVICE_CONFIG*>(new BYTE[dwBytesNeeded]);
	memset(pServiceConfig, 0, dwBytesNeeded);
	if (!QueryServiceConfig(scTargetService, pServiceConfig, dwBytesNeeded, &dwBytesNeeded))
	{
		delete pServiceConfig;
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	if (pServiceConfig->dwStartType == SERVICE_DISABLED)
	{
		delete pServiceConfig;
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return true;
	}
	delete pServiceConfig;
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return false;
}

bool ServiceManager::IsServiceStopped(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_READ | SC_MANAGER_CONNECT);
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_INTERROGATE | SERVICE_QUERY_STATUS);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is started already or not .. try to start if so
	SERVICE_STATUS serviceStatus;
	memset(&serviceStatus, 0, sizeof(SERVICE_STATUS));
	if (!QueryServiceStatus(scTargetService, &serviceStatus))
	{
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// don't allow pending because want to know if it is done stopping
	if (serviceStatus.dwCurrentState != SERVICE_STOPPED) //|| serviceStatus.dwCurrentState!=SERVICE_START_PENDING)
	{
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return true;
}

bool ServiceManager::IsServiceStarted(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_READ | SC_MANAGER_CONNECT);
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_INTERROGATE | SERVICE_QUERY_STATUS);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is started already or not .. try to start if so
	SERVICE_STATUS serviceStatus;
	memset(&serviceStatus, 0, sizeof(SERVICE_STATUS));
	if (!QueryServiceStatus(scTargetService, &serviceStatus))
	{
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// don't allow pending because want to know if it is done starting
	if (serviceStatus.dwCurrentState != SERVICE_RUNNING) //|| serviceStatus.dwCurrentState!=SERVICE_START_PENDING)
	{
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return true;
}

