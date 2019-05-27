#pragma once

// This is very old code, needs a refresh and some proper TLC

#define TERM_SERVICE_NAME_XP L"TermService"

using namespace std;

class CMyServiceInfo
{
public:
	CString csServiceName;
	CString csFriendlyName;
	SERVICE_STATUS_PROCESS serviceStatus;
};

class ServiceManager
{
	int m_nCurrentEnumerationPointer; // into below array
	CRITICAL_SECTION m_critServicesEnumerationVectors;
	vector<CString> m_vcsServiceNames;
	vector<CString> m_vcsServiceDisplayNames;
	vector<SERVICE_STATUS_PROCESS> m_vnServiceStatuses;

	bool EnumerateServiceNames(); // use internally for get/next
	bool m_bRefreshNeeded;	// true if refresh needed (known new process created, or other known change)

public:
	ServiceManager()
	{
		InitializeCriticalSection(&m_critServicesEnumerationVectors);
		m_nCurrentEnumerationPointer = 0;
		m_bRefreshNeeded = true;
	}
	~ServiceManager()
	{
		DeleteCriticalSection(&m_critServicesEnumerationVectors);
	}
	///////////////////////////////////////////////////////////////////////////////////////
	// MakeSureServiceIsEnabledAndStarted
	// 
	// These function change the state if not as desired (if permission exist).
	//

	void NotifyRefreshNeeded();

	bool DoesServiceExist(const WCHAR *ptszServiceName);
	bool Stop(const WCHAR *ptszServiceName, bool bWait);
	bool IsServiceStopped(const WCHAR *ptszServiceName);
	bool Start(const WCHAR *ptszServiceName, bool bWait);
	bool MakeSureServiceIsEnabled(const WCHAR *ptszServiceName);
	bool MakeSureServiceIsStarted(const WCHAR *ptszServiceName, bool bWaitForStart = false);
	bool IsServiceDisabled(const WCHAR *ptszServiceName);
	bool IsServiceStarted(const WCHAR *ptszServiceName);	

	bool GetFirstServiceByName(WCHAR *ptszServiceName, int ncch, WCHAR *ptszServiceDisplayName, int ncch2, SERVICE_STATUS_PROCESS *pnServiceStatus);
	bool GetNextServiceByName(WCHAR *ptszServiceName, int ncch, WCHAR *ptszServiceDisplayName, int ncch2, SERVICE_STATUS_PROCESS *pnServiceStatus);

	bool GetServiceNameForProcessID(DWORD dwPID, const WCHAR *ptszServiceName);
	bool IsProcessAService(DWORD dwPID, vector<CMyServiceInfo> *pvcMyServiceInfo, bool bForceReenumeration = false);
};
