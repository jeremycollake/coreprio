#pragma once
#include "LogOut.h"
#include <set>
#include <atlstr.h>

#define COREPRIO_MINIMUM_REFRESH_RATE_MS (100)
#define COREPRIO_DEFAULT_REFRESH_RATE (500)
#define COREPRIO_DEFAULT_INCLUSION_PATTERN L"*"
#define COREPRIO_DEFAULT_EXCLUSION_PATTERN L""

#define verbosewprintf

class ThreadManager
{
	SYSTEM_INFO sysInfo;
	HANDLE hExitEvent;
	HANDLE hFinishedEvent;
	LogOut Log;
	BOOL bApplyNUMAFix;
	BOOL bEnableDLM;
	unsigned long nRefreshRate;
	unsigned int nThreadsToManage;
	unsigned __int64 bitvPrioritizedAffinity;
	CString csIncludes;
	CString csExcludes;

	// TIDs being managed
	// we only need to record TIDs since we exclude any thread that doesn't have an ALL cpu affinity, thus original affinity to restore implied (all)
	std::set<DWORD> ThreadsActivelyManaged;
	
	DWORD_PTR GetThreadAffinityMask(HANDLE hThread);
	bool IsProcessIncluded(const WCHAR *pwszProcessName, CString csInclusionPattern, CString csExclusionPattern);
	bool DoesStringMatchDelimitedPattern(const WCHAR *pwszMatch, CString csPattern);
	
	void UnmanageAllThreads();

public:
	ThreadManager(BOOL bEnableDLM, unsigned long nRefreshRate, unsigned int nThreadsToManage, unsigned __int64 bitvPrioritizedAffinity, BOOL bApplyNUMAFix, CString csIncludes, CString csExcludes, LogOut::LOG_TARGET logTarget);
	ThreadManager();
	~ThreadManager();
	bool Begin(HANDLE hExitEvent);
	void End();

	unsigned long GetDefaultRefreshRate();
	unsigned long GetDefaultThreadsToManage();
	unsigned __int64 GetDefaultPrioritizedAffinity();	
	void GetDefaultMatchPatterns(CString &csIncludes, CString &csExcludes);
};

typedef struct _THREAD_TIME_INFO
{
	unsigned __int64 ftCreation;
	unsigned __int64 ftExit;
	unsigned __int64 ftKernel;
	unsigned __int64 ftUser;
} THREAD_TIME_INFO;
