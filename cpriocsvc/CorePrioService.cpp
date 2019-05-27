#pragma region Includes 
#include "pch.h"
#include "CorePrioService.h" 
#include "ThreadPool.h" 
#include "../libcoreprio/ThreadManagement.h"
#include "../libcoreprio/ServiceManager.h"
#include "../libcoreprio/productoptions.h"

#pragma endregion 

CCorePrioService::CCorePrioService(const WCHAR *pszServiceName,
	BOOL fCanStop,
	BOOL fCanShutdown,
	BOOL fCanPauseContinue)
	: CServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
{	
	// Create a manual-reset event that is not signaled at first to indicate  
	// the stopped signal of the service. 
	m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);	
	m_hStoppingEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}


CCorePrioService::~CCorePrioService(void)
{
	if (m_hStoppedEvent)
	{
		CloseHandle(m_hStoppedEvent);
		m_hStoppedEvent = NULL;
	}
	if (m_hStoppingEvent)
	{
		CloseHandle(m_hStoppingEvent);
		m_hStoppingEvent = NULL;
	}
}

void CCorePrioService::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)
{
	// Log a service start message to the Application log. 
	WriteEventLogEntry(L"CorePrio OnStart",
		EVENTLOG_INFORMATION_TYPE);

	DEBUG_PRINT(L"OnStart");
	
	// launch a thread that watches AMD DLM service to make sure it isn't running
	// we do this periodically in case DLM is started after us.
	CThreadPool::QueueUserWorkItem(&CCorePrioService::ServiceWatchAMDDLMThread, this);

	// Queue the main service function for execution in a worker thread. 
	CThreadPool::QueueUserWorkItem(&CCorePrioService::ServiceWorkerThread, this);	
}

void CCorePrioService::ServiceWatchAMDDLMThread(void)
{
	DEBUG_PRINT(L"ServiceWatchAMDThreadThread");
	ServiceManager serviceManager;
	if (false == serviceManager.DoesServiceExist(AMD_DLM_SERVICE_NAME))
	{
		DEBUG_PRINT(L"AMD DLM service not installed. Not polling its status.");
		return;
	}
	else
	{
		do
		{
			if (serviceManager.IsServiceStarted(AMD_DLM_SERVICE_NAME))
			{
				DEBUG_PRINT(L"AMD DLM service found to be started. Stopping it!");
				serviceManager.Stop(AMD_DLM_SERVICE_NAME,false);
				WriteEventLogEntry(L"CorePrio Stopping AMD DLM",
					EVENTLOG_INFORMATION_TYPE);
			}
		} while (WaitForSingleObject(m_hStoppingEvent, 30 * 1000) == WAIT_TIMEOUT);
	}
	DEBUG_PRINT(L"ServiceWatchAMDDLMThread ending");
}

void CCorePrioService::ServiceWorkerThread(void)
{
	DEBUG_PRINT(L"Service Worker Thread");
	// Periodically check if the service is stopping. 
	ProductOptions prodOptions_machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
	BOOL bApplyNUMAFix = FALSE;
	BOOL bEnableDLM = FALSE;
	CString csInclusions, csExclusions;
	DWORD nRefreshRate = 0;
	DWORD nThreadCount = 0;
	unsigned long long bitvecAffinity = 0;
	
	if(false == prodOptions_machine.get_value(REG_VALUE_ENABLE_DLM, bEnableDLM))
	{
		bEnableDLM = FALSE;
	}
	DEBUG_PRINT(L"DLM is %s", bEnableDLM ? L"ENABLED" : L"DISABLED");
	
	if (false == prodOptions_machine.get_value(REG_VALUE_PRI_AFF, bitvecAffinity))
	{
		bitvecAffinity = 0;
	}
	DEBUG_PRINT(L"Service thread starting with prioritized affinity of %I64x", bitvecAffinity);
	
	if (false == prodOptions_machine.get_value(REG_VALUE_THREAD_COUNT, nThreadCount))
	{	
		nThreadCount = 0;
	}
	DEBUG_PRINT(L"Service thread starting with thread count of %u", nThreadCount);	

	if (false == prodOptions_machine.get_value(REG_VALUE_REFRESH_RATE, nRefreshRate))
	{
		nRefreshRate = 0;
	}
	DEBUG_PRINT(L"Service thread starting with refresh rate of %u", nRefreshRate);

	if (false == prodOptions_machine.get_value(REG_VALUE_APPLY_NUMA_FIX, bApplyNUMAFix))
	{	
		bApplyNUMAFix = FALSE;
	}
	DEBUG_PRINT(L"Service thread starting with NUMA fix setting being %s", bApplyNUMAFix ? L"APPLIED" : L"not applied");

	if (true == prodOptions_machine.get_value(REG_VALUE_INCLUSIONS, csInclusions))
	{
		DEBUG_PRINT(L"Loaded inclusion pattern %s", csInclusions);
	}
	if (true == prodOptions_machine.get_value(REG_VALUE_EXCLUSIONS, csExclusions))
	{
		DEBUG_PRINT(L"Loaded exclusion pattern %s", csExclusions);
	}
	
	ThreadManager *pThreadManager;
	pThreadManager = new ThreadManager(bEnableDLM, nRefreshRate, nThreadCount, bitvecAffinity, bApplyNUMAFix, csInclusions, csExclusions, LogOut::LTARGET_DEBUG);
	// returns when m_hStoppingEvent is signalled
	pThreadManager->Begin(m_hStoppingEvent);
	
	DEBUG_PRINT(L"Service Worker Thread ending");
	// Signal the stopped event. 
	SetEvent(m_hStoppedEvent);
}


void CCorePrioService::OnStop()
{
	// Log a service stop message to the Application log. 
	WriteEventLogEntry(L"CorePrio OnStop",
		EVENTLOG_INFORMATION_TYPE);

	// Indicate that the service is stopping and wait for the finish of the  
	// main service function (ServiceWorkerThread). 	
	SetEvent(m_hStoppingEvent);
	if (WaitForSingleObject(m_hStoppedEvent, INFINITE) != WAIT_OBJECT_0)
	{
		throw GetLastError();
	}
}