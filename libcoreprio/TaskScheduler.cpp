/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*
* Warning: This is legacy code. Quickly slapped together. May need maintenance.
* some code here derived from MSDN samples on the Task Scheduler 2 COM interface
*
*/
#include "stdafx.h"
#include <string>
#include <taskschd.h>
#include <comutil.h>
#include "TaskScheduler.h"

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsupp.lib")


CTaskScheduler::CTaskScheduler()
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

CTaskScheduler::~CTaskScheduler()
{
	CoUninitialize();
}

bool CTaskScheduler::CreateStartupTask(const TCHAR *ptszTaskName, const TCHAR *ptszPathname, const TCHAR *ptszCommandLine, const TCHAR *ptszUsername, bool bAllUsers, bool bElevated)
{
	HRESULT hr;
	CString wsTemp;	

	_ASSERT(ptszTaskName);
	if (NULL == ptszTaskName)
	{
		return false;
	}

	//  ------------------------------------------------------
	//  Create a name for the task.
	LPCWSTR wszTaskName = ptszTaskName;
	LPCWSTR wstrExecutablePath = ptszPathname;

	//  ------------------------------------------------------
	//  Create an instance of the Task Service. 
	ITaskService *pService = NULL;
	hr = CoCreateInstance(CLSID_TaskScheduler,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_ITaskService,
		(void**)&pService);
	if (FAILED(hr))
	{
		//"ERROR: %x\nEFFECT: %s can not start at login";
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);		
		return false;
	}

	//  Connect to the task service.
	hr = pService->Connect(_variant_t(), _variant_t(),
		_variant_t(), _variant_t());

	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pService->Release();		
		return false;
	}

	//  ------------------------------------------------------
	//  Get the pointer to the root task folder.  This folder will hold the
	//  new task that is registered.
	ITaskFolder *pRootFolder = NULL;
	hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pService->Release();		
		return false;
	}

	//  If the same task exists, remove it.
	pRootFolder->DeleteTask(_bstr_t(wszTaskName), 0);

	//  Create the task builder object to create the task.
	ITaskDefinition *pTask = NULL;
	hr = pService->NewTask(0, &pTask);

	pService->Release();  // COM clean up.  Pointer is no longer used.
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();		
		return false;
	}

	//  ------------------------------------------------------
	//  Get the registration info for setting the identification.
	IRegistrationInfo *pRegInfo = NULL;
	hr = pTask->get_RegistrationInfo(&pRegInfo);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();
		return false;
	}

	hr = pRegInfo->put_Author(bstr_t(L"Bitsum LLC"));
	pRegInfo->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  ------------------------------------------------------
	//  Create the settings for the task
	ITaskSettings *pSettings = NULL;
	hr = pTask->get_Settings(&pSettings);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  Set setting values for the task. 
	hr = pSettings->put_StartWhenAvailable(VARIANT_TRUE);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}
	pSettings->put_ExecutionTimeLimit(_bstr_t(_T("PT0S")));
	pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
	pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
	pSettings->put_MultipleInstances(TASK_INSTANCES_PARALLEL);
	pSettings->put_Enabled(VARIANT_TRUE);
	pSettings->Release();

	//
	//
	//  Set up principal information (security and more):
	//
	IPrincipal *pPrincipal = NULL;
	hr = pTask->get_Principal(&pPrincipal);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  Run the task with the set privileges (LUA) 
	hr = pPrincipal->put_RunLevel(bElevated ? TASK_RUNLEVEL_HIGHEST : TASK_RUNLEVEL_LUA);
	pPrincipal->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  ------------------------------------------------------
	//  Get the trigger collection to insert the logon trigger.
	ITriggerCollection *pTriggerCollection = NULL;
	hr = pTask->get_Triggers(&pTriggerCollection);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  Add the logon trigger to the task.
	ITrigger *pTrigger = NULL;
	hr = pTriggerCollection->Create(TASK_TRIGGER_LOGON, &pTrigger);
	pTriggerCollection->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	ILogonTrigger *pLogonTrigger = NULL;
	hr = pTrigger->QueryInterface(
		IID_ILogonTrigger, (void**)&pLogonTrigger);
	pTrigger->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}	

	hr = pLogonTrigger->put_Id(_bstr_t(L"Trigger1"));
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pLogonTrigger->Release();
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	if (ptszUsername || bAllUsers)
	{
		wsTemp.Format(L"\\\\.\\%s", ptszUsername);
		wsTemp = ptszUsername;	// now pass fully qualified username
		hr = pLogonTrigger->put_UserId(_bstr_t(wsTemp));
		pLogonTrigger->Release();
		if (FAILED(hr))
		{
			DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
			pRootFolder->Release();
			pTask->Release();			
			return false;
		}
	}
	else
	{		
		hr = pLogonTrigger->put_UserId(NULL);
		pLogonTrigger->Release();
	}

	//  ------------------------------------------------------
	//  Add an Action to the task.  
	IActionCollection *pActionCollection = NULL;

	//  Get the task action collection pointer.
	hr = pTask->get_Actions(&pActionCollection);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  Create the action, specifying that it is an executable action.
	IAction *pAction = NULL;
	hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
	pActionCollection->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	IExecAction *pExecAction = NULL;
	//  QI for the executable task pointer.
	hr = pAction->QueryInterface(
		IID_IExecAction, (void**)&pExecAction);
	pAction->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	//  Set the path of the executable
	CString csTempExePath = wstrExecutablePath;
	CString csCommandLine = ptszCommandLine;
	csTempExePath.TrimLeft();
	// if not on a SMB path
	if (csTempExePath.GetAt(0) != L'\"')
	{
		csTempExePath.Format(L"\"%s\"", wstrExecutablePath);
	}
	csTempExePath.TrimLeft();
	hr = pExecAction->put_Path(_bstr_t(csTempExePath));
	pExecAction->put_Arguments(_bstr_t(csCommandLine));
	pExecAction->Release();
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR Task Scheduler %x", hr);
		pRootFolder->Release();
		pTask->Release();
		return false;
	}

	//  ------------------------------------------------------
	//  Save the task in the root folder.
	IRegisteredTask *pRegisteredTask = NULL;
		
	hr = pRootFolder->RegisterTaskDefinition(
		_bstr_t(wszTaskName),
		pTask,
		TASK_CREATE_OR_UPDATE,
		//_variant_t(L"S-1-5-32-544"), // administrators
		variant_t("S-1-5-32-545"), // users
		_variant_t(),
		TASK_LOGON_GROUP,
		_variant_t(L""),
		&pRegisteredTask);

	if (FAILED(hr))
	{
		DEBUG_PRINT(L"ERROR saving Task Scheduler task - %x", hr);
		pRootFolder->Release();
		pTask->Release();		
		return false;
	}

	// Clean up		
	pRootFolder->Release();
	pTask->Release();
	pRegisteredTask->Release();
	
	return true;
}

bool CTaskScheduler::RemoveStartupTask(LPCWSTR pwszTaskName, LPCWSTR pwszTaskPathnameOpt)
{	
	HRESULT hr;
	CString wsTemp;

	//  ------------------------------------------------------
	//  Create a name for the task.
	LPCWSTR wszTaskName = pwszTaskName;
	LPCWSTR wstrExecutablePath;
	if (pwszTaskPathnameOpt)
	{
		wstrExecutablePath = pwszTaskPathnameOpt;
	}

	//  ------------------------------------------------------
	//  Create an instance of the Task Service. 
	ITaskService *pService = NULL;
	hr = CoCreateInstance(CLSID_TaskScheduler,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_ITaskService,
		(void**)&pService);
	if (FAILED(hr))
	{
		return false;
	}

	//  Connect to the task service.
	hr = pService->Connect(_variant_t(), _variant_t(),
		_variant_t(), _variant_t());

	if (FAILED(hr))
	{
		pService->Release();		
		return false;
	}

	//  ------------------------------------------------------
	//  Get the pointer to the root task folder.  This folder will hold the
	//  new task that is registered.
	ITaskFolder *pRootFolder = NULL;
	hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
	if (FAILED(hr))
	{
		DEBUG_PRINT(L"Cannot get Root Folder pointer: %x\nEFFECT: Can not set Process Lasso to start at login with elevated rights!", hr);
		pService->Release();		
		return false;
	}

	// If the same task exists, remove it.
	// safety to ensure the task has a non-empty name
	if (wszTaskName[0])
	{
		pRootFolder->DeleteTask(_bstr_t(wszTaskName), 0);
	}
	else
	{
		DEBUG_PRINT(L"ERROR: Provided task name was empty. Not deleting.");
	}

	pRootFolder->Release();
	pService->Release();
	
	return true;
}

bool CTaskScheduler::EnsureTaskScheduler2ServiceIsEnabledAndReady()
{
	bool bR = false;
	SC_HANDLE scManager;

	scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scManager)
	{
		return bR;
	}

	SC_HANDLE scTaskScheduler = OpenService(scManager, _T("Schedule"), SERVICE_ALL_ACCESS);
	if (!scTaskScheduler)
	{
		CloseServiceHandle(scManager);		
		return bR;
	}

	if (!StartService(scTaskScheduler, 0, NULL))
	{
		switch (GetLastError())
		{
		case ERROR_SERVICE_ALREADY_RUNNING:			
			bR = true;
			break;
		case ERROR_SERVICE_DISABLED:			
			if (ChangeServiceConfig(scTaskScheduler, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
			{								
				if (StartService(scTaskScheduler, 0, NULL))
				{				
					bR = true;
				}
			}
			break;
		default:			
			break;
		}
	}

	CloseServiceHandle(scTaskScheduler);
	CloseServiceHandle(scManager);
	return bR;
}

