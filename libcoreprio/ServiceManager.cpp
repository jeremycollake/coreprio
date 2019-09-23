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

bool ServiceManager::MakeSureServiceIsEnabled(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_MODIFY_BOOT_CONFIG | SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); // TODO: redudency in permissions here
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS); //SERVICE_INTERROGATE|SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|SERVICE_START|SERVICE_STOP); 
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is enabled in its boot config, if not then enable it
	DWORD dwBytesNeeded = 0;
	QueryServiceConfig(scTargetService, NULL, 0, &dwBytesNeeded);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	dwBytesNeeded += 1024;	// in case it grows on us real fast
	// but MSDN says this is max size
	if (dwBytesNeeded > 8 * 1024)
	{
		dwBytesNeeded = 8 * 1024;
	}
	VOID *pserviceConfigRaw;
	pserviceConfigRaw = (VOID *)new BYTE[dwBytesNeeded];	// in case it grows
	memset(pserviceConfigRaw, 0, dwBytesNeeded);
	QUERY_SERVICE_CONFIG *pServiceConfig = (QUERY_SERVICE_CONFIG *)pserviceConfigRaw;
	if (!QueryServiceConfig(scTargetService, pServiceConfig, dwBytesNeeded, &dwBytesNeeded))
	{
		delete pserviceConfigRaw;		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	if (pServiceConfig->dwStartType == SERVICE_DISABLED)
	{		
		pServiceConfig->dwStartType = SERVICE_DEMAND_START;
		//serviceConfig.dwStartType=SERVICE_AUTO_START;
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
			delete pserviceConfigRaw;
			CloseServiceHandle(scTargetService);
			CloseServiceHandle(scServiceManager);
			return false;
		}
		NotifyBootConfigStatus(TRUE);
	}
	delete pserviceConfigRaw;
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

bool ServiceManager::Stop(const WCHAR *ptszServiceName, const bool bWait)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_MODIFY_BOOT_CONFIG | SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); // TODO: redudency in permissions here
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS); //SERVICE_INTERROGATE|SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|SERVICE_START|SERVICE_STOP); 
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
		if (!::ControlService(scTargetService, SERVICE_CONTROL_STOP, &serviceStatus))
		{
			CloseServiceHandle(scTargetService);
			CloseServiceHandle(scServiceManager);
			return false;
		}
	}
	unsigned nI = 0;
	if (bWait)
	{
		// service handles are NOT waitable objects
		// MSDN says to use polling, so we will
		// there a hint provided in the service config, but probably useless
		// we're using a max of 1 minute to start
		for (nI = 0; nI < 10; nI++)
		{
			Sleep(500);
			if (IsServiceStopped(ptszServiceName))
			{
				break;
			}
		}
	}
	bool bR = false;
	if (nI >= 10)
	{		
		bR = false;
	}
	else
	{		
		bR = true;
	}
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return bR;

}

bool ServiceManager::Start(const WCHAR *ptszServiceName, const bool bWait)
{
	return MakeSureServiceIsStarted(ptszServiceName, bWait);
}

bool ServiceManager::MakeSureServiceIsStarted(const WCHAR *ptszServiceName, const bool bWait)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_MODIFY_BOOT_CONFIG | SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); // TODO: redudency in permissions here
	if (!scServiceManager)
	{
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS); //SERVICE_INTERROGATE|SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|SERVICE_START|SERVICE_STOP); 
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
			if (!::StartService(scTargetService, 0, NULL))
			{				
				CloseServiceHandle(scTargetService);
				CloseServiceHandle(scServiceManager);
				return false;
			}
		}
	}
	unsigned nI = 0;
	if (bWait)
	{
		DEBUG_PRINT(L"ServiceManager - waiting for start to complete");
		// service handles are NOT waitable objects
		// MSDN says to use polling, so we will
		// there a hint provided in the service config, but probably useless
		// we're using a max of 30 seconds to start
		Sleep(1000);	// do an initial 1 sec sleep, so first wait is 2x		
		for (nI = 0; nI < 30; nI++)
		{
			Sleep(1000);
			if (IsServiceStarted(ptszServiceName))
			{
				break;
			}
		}
	}
	_ASSERT(nI < 60);
	if (nI >= 60)
	{
		DEBUG_PRINT(L"ServiceManager - gave up waiting!!");
	}
	else
	{
		DEBUG_PRINT(L"ServiceManager - started %s", ptszServiceName);
	}
	CloseServiceHandle(scTargetService);
	CloseServiceHandle(scServiceManager);
	return true;
}

bool ServiceManager::IsServiceDisabled(const WCHAR *ptszServiceName)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, GENERIC_READ);
	if (!scServiceManager)
	{		
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, STANDARD_RIGHTS_READ | SERVICE_QUERY_CONFIG);
	if (!scTargetService)
	{
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is enabled in its boot config, if not then enable it
	DWORD dwBytesNeeded = 0;
	QueryServiceConfig(scTargetService, NULL, 0, &dwBytesNeeded);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	dwBytesNeeded += 1024;	// in case it grows on us real fast
	// but MSDN says this is max size
	if (dwBytesNeeded > 8 * 1024)
	{
		dwBytesNeeded = 8 * 1024;
	}
	VOID *pserviceConfigRaw;
	pserviceConfigRaw = (VOID *)new BYTE[dwBytesNeeded];	// in case it grows
	memset(pserviceConfigRaw, 0, dwBytesNeeded);
	QUERY_SERVICE_CONFIG *pServiceConfig = (QUERY_SERVICE_CONFIG *)pserviceConfigRaw;
	if (!QueryServiceConfig(scTargetService, pServiceConfig, dwBytesNeeded, &dwBytesNeeded))
	{
		delete pserviceConfigRaw;		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return false;
	}
	if (pServiceConfig->dwStartType == SERVICE_DISABLED)
	{
		delete pserviceConfigRaw;		
		CloseServiceHandle(scTargetService);
		CloseServiceHandle(scServiceManager);
		return true;
	}
	delete pserviceConfigRaw;	
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
	// don't allow pending because want to know if it is done starting
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

