/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#include "stdafx.h"
#include "ServiceManager.h"

void ServiceManager::NotifyRefreshNeeded()
{
	m_bRefreshNeeded = true;
}
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
		// TODO: We need 
		//DbgPrintf(L"Service disabled, enabling it for on-demand starting...");
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

bool ServiceManager::Stop(const WCHAR *ptszServiceName, bool bWait)
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

bool ServiceManager::Start(const WCHAR *ptszServiceName, bool bWait)
{
	return MakeSureServiceIsStarted(ptszServiceName, bWait);
}

bool ServiceManager::MakeSureServiceIsStarted(const WCHAR *ptszServiceName, bool bWait)
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, SC_MANAGER_MODIFY_BOOT_CONFIG | SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE); // TODO: redudency in permissions here
	if (!scServiceManager)
	{
		//DbgPrintf(L"ServiceManager - ");
		return false;
	}
	// now open the target service
	SC_HANDLE scTargetService = OpenService(scServiceManager, ptszServiceName, SERVICE_ALL_ACCESS); //SERVICE_INTERROGATE|SERVICE_QUERY_CONFIG|SERVICE_QUERY_STATUS|SERVICE_START|SERVICE_STOP); 
	if (!scTargetService)
	{
		//DbgPrintf(L"ServiceManager - ");
		CloseServiceHandle(scServiceManager);
		return false;
	}
	// see if is started already or not .. try to start if so
	SERVICE_STATUS serviceStatus;
	memset(&serviceStatus, 0, sizeof(SERVICE_STATUS));
	if (!QueryServiceStatus(scTargetService, &serviceStatus))
	{
		//DbgPrintf(L"ServiceManager - ");
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
				//DbgPrintf(L"ServiceManager - ");
				CloseServiceHandle(scTargetService);
				CloseServiceHandle(scServiceManager);
				return false;
			}
		}
	}
	unsigned nI = 0;
	if (bWait)
	{
		//DbgPrintf(L"ServiceManager - starting - waiting to complete");
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
		//DbgPrintf(L"ServiceManager - gave up waiting!!");
	}
	else
	{
		//DbgPrintf(L"ServiceManager - started");
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

bool ServiceManager::GetFirstServiceByName(WCHAR *ptszServiceName, int ncch, WCHAR *ptszServiceDisplayName, int ncch2, SERVICE_STATUS_PROCESS *pnServiceStatus)
{
	m_nCurrentEnumerationPointer = 0;
	EnumerateServiceNames();
	return GetNextServiceByName(ptszServiceName, ncch, ptszServiceDisplayName, ncch2, pnServiceStatus);
}

bool ServiceManager::GetNextServiceByName(WCHAR *ptszServiceName, int ncch, WCHAR *ptszServiceDisplayName, int ncch2, SERVICE_STATUS_PROCESS *pnServiceStatus)
{
	EnterCriticalSection(&m_critServicesEnumerationVectors);
	if (m_nCurrentEnumerationPointer < (int)m_vcsServiceNames.size())
	{
		wcsncpy_s(ptszServiceName, ncch, m_vcsServiceNames[m_nCurrentEnumerationPointer].GetBuffer(), _TRUNCATE);
		wcsncpy_s(ptszServiceDisplayName, ncch2, m_vcsServiceDisplayNames[m_nCurrentEnumerationPointer].GetBuffer(), _TRUNCATE);
		*pnServiceStatus = m_vnServiceStatuses[m_nCurrentEnumerationPointer];
		// do AFTERWARDS
		m_nCurrentEnumerationPointer++;
		LeaveCriticalSection(&m_critServicesEnumerationVectors);
		return true;
	}
	LeaveCriticalSection(&m_critServicesEnumerationVectors);
	return false;
}



bool ServiceManager::EnumerateServiceNames()
{
	SC_HANDLE scServiceManager = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_READ | SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
	if (!scServiceManager)
	{
		return false;
	}

	// then simply use to fill m_vcsServiceNames
	// EnumServicesStatusEx	
	size_t nCurrentSize = 64 * 1024;
	BYTE *pBuffer = (BYTE *)malloc(nCurrentSize);
	DWORD dwBytesNeeded = 0;
	DWORD dwServicesReturned = 0;
	DWORD dwCurrentEnumerationIndex_ResumeHandle = 0;	//mandatory
	while (EnumServicesStatusEx(scServiceManager,
		SC_ENUM_PROCESS_INFO,
		SERVICE_WIN32,			// NOTE - TODO - This is just giving Win32 services, do we want Kernel services too?
		SERVICE_STATE_ALL,
		pBuffer,
		(DWORD)nCurrentSize,
		&dwBytesNeeded,
		&dwServicesReturned,
		&dwCurrentEnumerationIndex_ResumeHandle,
		NULL) == ERROR_MORE_DATA) // group name
	{
		_ASSERT(dwBytesNeeded > nCurrentSize);
		nCurrentSize = dwBytesNeeded;
		pBuffer = (BYTE *)realloc(pBuffer, nCurrentSize);
	}
	// no services
	if (!dwServicesReturned)
	{
		if (pBuffer) free(pBuffer);
		return false;
	}

	EnterCriticalSection(&m_critServicesEnumerationVectors);
	// 
	// we now have all service names and info, so just go through it
	//
	ENUM_SERVICE_STATUS_PROCESS *pServices = (ENUM_SERVICE_STATUS_PROCESS *)pBuffer;
	for (unsigned int nI = 0; nI < dwServicesReturned; nI++)
	{
		m_vcsServiceNames.push_back(pServices->lpServiceName);
		m_vcsServiceDisplayNames.push_back(pServices->lpDisplayName);
		m_vnServiceStatuses.push_back(pServices->ServiceStatusProcess);
		pServices++;
	}
	LeaveCriticalSection(&m_critServicesEnumerationVectors);
	free(pBuffer);
	m_bRefreshNeeded = false;
	return true;
}

// can return a linked list - so requires custom dealloc function
bool ServiceManager::IsProcessAService(DWORD dwPID, vector<CMyServiceInfo> *pvcMyServiceInfo, bool bForceReenumeration)
{
	if (!m_vnServiceStatuses.size() || bForceReenumeration || m_bRefreshNeeded)
	{
		EnumerateServiceNames();
	}
	EnterCriticalSection(&m_critServicesEnumerationVectors);
	unsigned int nFoundCount = 0;
	for (unsigned int nI = 0; nI < m_vnServiceStatuses.size(); nI++)
	{
		if (m_vnServiceStatuses[nI].dwProcessId == dwPID)
		{
			// ust want yes or no, so giev it to them
			if (!pvcMyServiceInfo)
			{
				LeaveCriticalSection(&m_critServicesEnumerationVectors);
				return true;
			}
			CMyServiceInfo cInfo;
			memcpy(&cInfo.serviceStatus, &m_vnServiceStatuses[nI], sizeof(SERVICE_STATUS_PROCESS));
			cInfo.csFriendlyName = m_vcsServiceDisplayNames[nI];
			cInfo.csServiceName = m_vcsServiceNames[nI];
			pvcMyServiceInfo->push_back(cInfo);
			nFoundCount++;	// now next time allocate
		}
	}
	//
	// TODO deallocation of list handled by overload of 'delete' operator on class, but may want to make sure
	// it works in a unit test... 
	// ^^ NOT WORKING - CALLLER *MUST* invoke Deallocate method to clear the results
	//		
	LeaveCriticalSection(&m_critServicesEnumerationVectors);
	return nFoundCount ? true : false;
}