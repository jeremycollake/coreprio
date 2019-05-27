#pragma once;
#include <cstring>

using namespace std;

#pragma comment(lib, "comsuppw.lib") 

class CTaskScheduler
{
public:
	CTaskScheduler();
	~CTaskScheduler();
	// task scheduler service manipulation
	bool IsTaskScheduler2ServiceStarted();
	bool StartTaskScheduler2Service();

	bool StartTask(const TCHAR *ptszTaskName);

	// task creation, removal, enumeration
	bool CreateStartupTask(const TCHAR *ptszTaskName, const TCHAR *ptszPathname, const TCHAR *ptszCommandLine, const TCHAR *ptszUsername, bool bAllUsers, bool bElevated);	
	bool RemoveStartupTask(LPCWSTR ptszTaskName, LPCWSTR pwszExecutablePathname = NULL);
	bool EnsureTaskScheduler2ServiceIsEnabledAndReady();	
};