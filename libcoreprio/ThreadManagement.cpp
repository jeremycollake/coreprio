/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#include "stdafx.h"
#include "ThreadManagement.h"
#include "misc.h"

ThreadManager::ThreadManager(BOOL bEnableDLM, unsigned long nRefreshRate, unsigned int nThreadsToManage, unsigned __int64 bitvPrioritizedAffinity, BOOL bApplyNUMAFix, CString csIncludes, CString csExcludes, LogOut::LOG_TARGET logTarget)
	: bEnableDLM (bEnableDLM), nRefreshRate (nRefreshRate), nThreadsToManage (nThreadsToManage) , bApplyNUMAFix(bApplyNUMAFix), bitvPrioritizedAffinity (bitvPrioritizedAffinity), csIncludes (csIncludes), csExcludes (csExcludes)
{		
	GetSystemInfo(&sysInfo);
	hExitEvent = NULL;
	hFinishedEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	Log.logTarget = logTarget;	
}

ThreadManager::ThreadManager()
{		
	// init to 0 forces defaults on Begin
	nRefreshRate = 0;
	nThreadsToManage = 0;
	bitvPrioritizedAffinity = 0;
	bEnableDLM = false;
	bApplyNUMAFix = false;

	GetSystemInfo(&sysInfo);
	Log.logTarget = LogOut::LTARGET_DEBUG;
	hExitEvent = NULL;
	hFinishedEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
}

ThreadManager::~ThreadManager()
{
	End();	
	CloseHandle(hFinishedEvent);
}

void ThreadManager::End()
{	
	// if Begin has been called, hExitEvent will be non-NULL
	if (hExitEvent)
	{
		SetEvent(hExitEvent);
		if (WaitForSingleObject(hFinishedEvent, nRefreshRate * 4) == WAIT_TIMEOUT)
		{
			_ASSERT(0);
			Log.Write(L"WARNING: Finished event wait timeout");
		}
	}
	UnmanageAllThreads();
}

bool ThreadManager::Begin(HANDLE hEvent)
{
	_ASSERT(hExitEvent == NULL);
	
	hExitEvent = hEvent;
	ResetEvent(hFinishedEvent);

	HANDLE hSnapshot = INVALID_HANDLE_VALUE;
	THREADENTRY32 te32;
				
	std::map<DWORD, CString> mapProcesses;	// PID to base name, all seen processes	this enum (cleared each iter)
	std::map<DWORD, THREAD_TIME_INFO> mapThreadTimes;	// TID to thread timing
	std::map<DWORD, unsigned int> mapThreadUtilization;	// TID to thread CPU utilization
	std::map<DWORD, unsigned int> mapProcessesFixed; // NUMA Dissociater map

	if (nRefreshRate < COREPRIO_MINIMUM_REFRESH_RATE_MS)
	{
		nRefreshRate = GetDefaultRefreshRate();
	}

	if (0 == nThreadsToManage)
	{
		nThreadsToManage = GetDefaultThreadsToManage();
	}
	_ASSERT(nThreadsToManage);

	if (0 == bitvPrioritizedAffinity)
	{
		bitvPrioritizedAffinity = GetDefaultPrioritizedAffinity();
	}

	// if both inclusion and exclusion are empty, assume default
	if (csExcludes.IsEmpty() && csIncludes.IsEmpty())
	{
		GetDefaultMatchPatterns(csIncludes, csExcludes);
		Log.Write(L"\n Default exclusion patterns used, inc: %s - exc:%s", csIncludes, csExcludes);
	}

	_ASSERT(bitvPrioritizedAffinity);
	_ASSERT(nRefreshRate);
	_ASSERT(nThreadsToManage);

	if (0 == nRefreshRate)
	{
		Log.Write(L"\nERROR: Refresh rate not set. Unable to begin.");
		return false;
	}

	if (0 == nThreadsToManage)
	{
		Log.Write(L"\nERROR: Threads to manage is 0. Unable to begin.");
		return false;
	}

	if (0 == bitvPrioritizedAffinity)
	{
		Log.Write(L"ERROR: No prioritized affinity set (0).");
		return false;
	}
	
	Log.Write(L"\n Refresh rate is %u ms", nRefreshRate);
	Log.Write(L"\n Managing up to %u software threads simultaneously", nThreadsToManage);
	Log.Write(L"\n System has %d logical CPU cores and an active mask of %08X%08X",
		sysInfo.dwNumberOfProcessors,
		static_cast<UINT32>((sysInfo.dwActiveProcessorMask >> 32) & 0xFFFFFFFF), static_cast<UINT32>(sysInfo.dwActiveProcessorMask) & 0xFFFFFFFF);
	Log.Write(L"\n Using prioritized affinity of %08X%08X",
		static_cast<UINT32>((bitvPrioritizedAffinity >> 32) & 0xFFFFFFFF), static_cast<UINT32>(bitvPrioritizedAffinity) & 0xFFFFFFFF);
	Log.Write(L"\n NUMA Fix is %s", bApplyNUMAFix ? L"APPLIED" : L"NOT applied");
	Log.Write(L"\n Inclusion pattern: %s", csIncludes);
	Log.Write(L"\n Exclusion pattern: %s", csExcludes);
	Log.Write(L"\n");

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	unsigned __int64 ftLastIteration = 0;	
	unsigned int nI = 0;
	do
	{
		// Take a snapshot of all processes, and threads if DLM enabled
		hSnapshot = CreateToolhelp32Snapshot((bEnableDLM ? TH32CS_SNAPTHREAD : 0) | TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
		{
			return(FALSE);
		}

		if (hSnapshot)
		{			
			std::map<DWORD, CString> mapIncludedProcesses;	// PID to base name, processes matched to pattern - reset every iteration
			int nExcludedProcessesCount = 0;

			PROCESSENTRY32 pe32;
			pe32.dwSize = sizeof(PROCESSENTRY32);
			if (Process32First(hSnapshot, &pe32))
			{
				do
				{					
					mapProcesses[pe32.th32ProcessID] = pe32.szExeFile;

					if (IsProcessIncluded(pe32.szExeFile, csIncludes, csExcludes))
					{
						mapIncludedProcesses[pe32.th32ProcessID] = pe32.szExeFile;
					}
					else
					{
						verbosewprintf(L"\n Process %s is excluded by match patterns", pe32.szExeFile);
						nExcludedProcessesCount++;
					}

					if (bApplyNUMAFix
						&&
						mapIncludedProcesses.find(pe32.th32ProcessID) != mapIncludedProcesses.end())
					{
						int nTimesApplied = 0;

						if (mapProcessesFixed.find(pe32.th32ProcessID) == mapProcessesFixed.end())
						{
							nTimesApplied = 0;
						}
						else
						{
							nTimesApplied = mapProcessesFixed[pe32.th32ProcessID];
						}
						if (0 == nTimesApplied)
						{
							unsigned __int64 bitvecProcessAffinity = 0;
							unsigned __int64 bitvecSystemAffinity = 0;

							HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
							if (hProcess)
							{
								GetProcessAffinityMask(hProcess, static_cast<PDWORD_PTR>(&bitvecProcessAffinity), static_cast<PDWORD_PTR>(&bitvecSystemAffinity));
								if (bitvecProcessAffinity && (bitvecProcessAffinity == bitvecSystemAffinity))
								{
									HANDLE hProcessSet = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
									if (hProcessSet)
									{
										Log.Write(L"\n Applying NUMA fix for process [%d] %s to %08X%08X", pe32.th32ProcessID, pe32.szExeFile, static_cast<UINT32>((bitvecProcessAffinity >> 32) & 0xFFFFFFFF), static_cast<UINT32>(bitvecProcessAffinity) & 0xFFFFFFFF);
										if (TRUE == SetProcessAffinityMask(hProcessSet, bitvecProcessAffinity))
										{

										}
										else
										{
											//Log.Write(L"\n WARNING: Could not set affinity mask for [%d] %s", pe32.th32ProcessID, pe32.szExeFile);
										}
										CloseHandle(hProcessSet);
									}
								}
								CloseHandle(hProcess);
							}
							// always record attempt so we don't keep retrying for non-all affinities or processes with errors
							mapProcessesFixed[pe32.th32ProcessID] = ++nTimesApplied;
						}
					}

				} while (Process32Next(hSnapshot, &pe32));
			}

			// for every historically 'fixed' process NOT in current map, delete it from historical tracking (because it terminated)
			std::set<DWORD> toErase;
			for (auto& it : mapProcessesFixed)
			{
				if (mapProcesses.find(it.first) == mapProcesses.end())
				{
					toErase.insert(it.first);					
				}
			}
			for (auto& it : toErase)
			{
				mapProcessesFixed.erase(it);
			}
				
			_ASSERT(mapProcessesFixed.size() <= mapProcesses.size());

			if (bEnableDLM)
			{
				// Fill in the size of the structure before using it. 
				te32.dwSize = sizeof(THREADENTRY32);

				// Retrieve information about the first thread,
				// and exit if unsuccessful
				if (!Thread32First(hSnapshot, &te32))
				{
					Log.FormattedErrorOut(L"Thread32First");
					CloseHandle(hSnapshot);
					return(FALSE);
				}

				float fUtilizationSumCheck = 0;
				do
				{
					if (mapIncludedProcesses.find(te32.th32OwnerProcessID) != mapIncludedProcesses.end())
					{						
						HANDLE hThread = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);						
						if(hThread)
						{
							PROCESSOR_NUMBER IdealCPU;
							memset(&IdealCPU, 0, sizeof(IdealCPU));
							unsigned __int64 ThreadMask = GetThreadAffinityMask(hThread);
							if (0 == GetThreadIdealProcessorEx(hThread, &IdealCPU))
							{
								// failed
							}
							verbosewprintf(L"\n %s PID: 0x%08X TID: 0x%08X ideal_cpu: g%hu #%u cpu_mask: %08X%08X",
								mapProcesses[te32.th32OwnerProcessID].GetBuffer(),
								te32.th32OwnerProcessID, te32.th32ThreadID,
								IdealCPU.Group, IdealCPU.Number,
								static_cast<UINT32>((ThreadMask >> 32) & 0xFFFFFFFF), static_cast<UINT32>(ThreadMask) & 0xFFFFFFFF);

							// get current thread time
							THREAD_TIME_INFO currentThreadTime;
							GetThreadTimes(hThread,
								(LPFILETIME)&currentThreadTime.ftCreation,
								(LPFILETIME)&currentThreadTime.ftExit,
								(LPFILETIME)&currentThreadTime.ftKernel,
								(LPFILETIME)&currentThreadTime.ftUser);

							// if we have recorded times in last iteration, use it to calculate CPU %
							unsigned int cpuUtil = 0;	// CPU utilization % * 100 to force integer
							std::map<DWORD, THREAD_TIME_INFO>::iterator iLastTime = mapThreadTimes.find(te32.th32ThreadID);
							if (iLastTime != mapThreadTimes.end())
							{
								// CPU % Busy =  (IntervalDuration – Idle Time) * 100 /  IntervalDuration )	
								unsigned __int64 ftCurrentIteration = 0;
								GetSystemTimeAsFileTime((LPFILETIME)&ftCurrentIteration);
								unsigned __int64 ftKernelDiff = currentThreadTime.ftKernel - iLastTime->second.ftKernel;
								unsigned __int64 ftUserDiff = currentThreadTime.ftUser - iLastTime->second.ftUser;
								unsigned __int64 ftIntervalTime = ftCurrentIteration - ftLastIteration;
								unsigned __int64 ftIdle = ftIntervalTime - (ftKernelDiff + ftUserDiff);
								cpuUtil = (unsigned int)((ftIntervalTime - ftIdle) * 100 * 100 / ftIntervalTime);  // extra *100 to further increase precision while using integers only (e.g. 24.01% becomes 2401)				
								if (cpuUtil > 0)
								{
									verbosewprintf(L"\n %04x relative utilization (as integer) %u", te32.th32ThreadID, cpuUtil);
								}
							}

							mapThreadTimes[te32.th32ThreadID] = currentThreadTime;

							// now update the thread utilization map, which will also represent a list of current threads
							mapThreadUtilization[te32.th32ThreadID] = cpuUtil;

							CloseHandle(hThread);
						} // end if able to open thread
					} // end if thread is part of a included process
					nI++;
				} while (Thread32Next(hSnapshot, &te32));

				std::vector<DWORD> vThreadsToDelete;
				// remove any threads not currently running from the historical tracking
				for (auto i = mapThreadTimes.begin();
					i != mapThreadTimes.end();
					++i)
				{
					auto i2 = mapThreadUtilization.find(i->first);
					if (i2 == mapThreadUtilization.end())
					{
						verbosewprintf(L"\n - Removing terminated thread 0x%08X", i->first);
						vThreadsToDelete.push_back(i->first);
					}
				}
				for (int nI = 0; nI < vThreadsToDelete.size(); nI++)
				{
					mapThreadTimes.erase(vThreadsToDelete[nI]);
				}
				
				
				// flip TID->CPU map into a multimap (since CPU use is not unique to a TID) so it is sorted ascending by CPU_UTIL->TID
				std::multimap<unsigned int, DWORD> UseToTID;
				for (auto& it : mapThreadUtilization)
				{
					UseToTID.insert(std::pair<unsigned int, DWORD>(it.second, it.first));
				}

				// quick vectors to track TIDs we are about to manage or unmanage
				std::set<DWORD> PendingThreadsToManage;
				std::set<DWORD> PendingThreadsToUnmanage;			
								
				for (std::multimap<unsigned int, DWORD>::reverse_iterator it = UseToTID.rbegin();
					it != UseToTID.rend() 
						&& std::distance(UseToTID.rbegin(), it)<nThreadsToManage	// only identify top X threads
						&& it->first>0;												// and stop when no more threads with >0% CPU time
					++it)
				{
					verbosewprintf(L"\n candidate %04X %u", it->second, it->first);
					PendingThreadsToManage.insert(it->second);										
				}

				// make a list of threads we will no longer track since they have diminished from the top threads list				
				for (auto& i : ThreadsActivelyManaged)
				{					
					// see if existing managed thread is in pending management set					
					if (PendingThreadsToManage.find(i)==PendingThreadsToManage.end())
					{
						// not in pending management list, so mark for pending unmanagement
						PendingThreadsToUnmanage.insert(i);
					}
				}

				if (ThreadsActivelyManaged.size())
				{
					verbosewprintf(L"\n\n TIDs: ");
					for (auto& c : ThreadsActivelyManaged) verbosewprintf(L"0x%04x ", c);
				}
				if (PendingThreadsToUnmanage.size())
				{
					verbosewprintf(L"\n\n Pending TIDs to unmanage:\n");
					for (auto& c : PendingThreadsToUnmanage) verbosewprintf(L"0x%04x ", c);
				}
				if (PendingThreadsToManage.size())
				{
					verbosewprintf(L"\n\n Pending TIDs to manage:\n");
					for (auto& c : PendingThreadsToManage) verbosewprintf(L"0x%04x ", c);
				}

				// remove threads to unmanage from active management
				for (auto& c : PendingThreadsToUnmanage)
				{
					auto i = ThreadsActivelyManaged.find(c);
					_ASSERT(i != ThreadsActivelyManaged.end());

					// restore affinity to 'all cpus'
					HANDLE hThread = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, c);
					if (hThread)
					{
						unsigned __int64 dwPriorAffinity = SetThreadAffinityMask(hThread, sysInfo.dwActiveProcessorMask);
						if (0 == dwPriorAffinity)
						{
							Log.Write(L"\n ! WARNING: Could not restore affinity for TID %04x", c);
						}
						CloseHandle(hThread);
					}

					// remove from set
					ThreadsActivelyManaged.erase(i);
				}

				// add pending threads to active management
				// make sure failure count doesn't equal management count				
				for (auto &c : PendingThreadsToManage)
				{					
					// set priotized affinity
					bool bSuccessfullySet = false;
					HANDLE hThread = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, c);					
					if (hThread)
					{
						unsigned __int64 dwPriorAffinity = SetThreadAffinityMask(hThread, bitvPrioritizedAffinity);
						if (0 == dwPriorAffinity)
						{
							//Log.Write(L"\n ! WARNING: Could not set prioritized affinity for TID 0x%04x (not necessarily abnormal)", c);							
						}
						else
						{
							bSuccessfullySet = true;
							//Log.Write(L"\n Set prioritized affinity for TID 0x%04x", c);
						}
						CloseHandle(hThread);
					}
					// add to vector if we successfully set
					if (bSuccessfullySet)
					{
						ThreadsActivelyManaged.insert(c);
					}
				}
				
				_ASSERT(ThreadsActivelyManaged.size() <= nThreadsToManage);
				
				GetSystemTimeAsFileTime((LPFILETIME)&ftLastIteration);
			}
			Log.Write(L"\n %u processes are excluded", nExcludedProcessesCount);
			CloseHandle(hSnapshot);
		}
		if (bEnableDLM)
		{
			Log.Write(L"\n Managing %u threads (of %u in %u processes) ...", ThreadsActivelyManaged.size(), mapThreadUtilization.size(), mapProcesses.size());
		}
		mapThreadUtilization.clear();
		mapProcesses.clear();

	} while (WaitForSingleObject(hExitEvent, nRefreshRate) == WAIT_TIMEOUT);

	UnmanageAllThreads();

	SetEvent(hFinishedEvent);

	return true;
}

void ThreadManager::UnmanageAllThreads()
{	
	Log.Write(L"\n Unmanaging all threads");
	// remove threads to unmanage from active management
	for (auto &c : ThreadsActivelyManaged)
	{
		// restore affinity to 'all cpus'
		HANDLE hThread = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, c);		
		if (hThread)
		{
			DWORD_PTR dwPriorAffinity = SetThreadAffinityMask(hThread, sysInfo.dwActiveProcessorMask);
			if (0 == dwPriorAffinity)
			{
				Log.Write(L"\n ! ERROR: Could not restore affinity for TID %04x", c);
			}
			CloseHandle(hThread);
		}
	}

	ThreadsActivelyManaged.clear();
}

unsigned __int64 ThreadManager::GetThreadAffinityMask(HANDLE hThread)
{
	// assume the process has a full CPU affinity as we probably don't want to act on the threads if it doesn't anyway
	// change the affinity mask to get current, then restore it
	DWORD_PTR OriginalMask = SetThreadAffinityMask(hThread, sysInfo.dwActiveProcessorMask);
	if (0 != OriginalMask)
	{
		if (0 == SetThreadAffinityMask(hThread, OriginalMask))
		{
			verbosewprintf(L"\n !! WARNING: Could not restore thread affinity");
		}
	}
	return (unsigned __int64)OriginalMask;
}


bool ThreadManager::IsProcessIncluded(const WCHAR* pwszProcessName, CString csIncludePattern, CString csExcludePattern)
{
	// match inclusion, then check for exclusion
	if (false == DoesStringMatchDelimitedPattern(pwszProcessName, csIncludePattern))
	{
		// does not match inclusions, return false
		return false;
	}
	if (false == DoesStringMatchDelimitedPattern(pwszProcessName, csExcludePattern))
	{
		// does not match exclusions, return true
		return true;
	}
	return false;
}

bool ThreadManager::DoesStringMatchDelimitedPattern(const WCHAR* pwszMatch, CString csPattern)
{
	//Log.Write(L"\n Checking %s to %s", csPattern, pwszMatch);
	int nPos = 0;
	CString csToken = csPattern.Tokenize(L";", nPos);
	while (-1 != nPos)
	{
		//Log.Write(L"\n (token) %s to %s", csToken, pwszMatch);
		if (wildicmpEx(csToken, pwszMatch))
		{
			return true;
		}
		csToken = csPattern.Tokenize(L";", nPos);
	};
	return false;
}

unsigned long ThreadManager::GetDefaultRefreshRate()
{
	return COREPRIO_DEFAULT_REFRESH_RATE;
}

unsigned long ThreadManager::GetDefaultThreadsToManage()
{
	return sysInfo.dwNumberOfProcessors / 2;
}

unsigned __int64 ThreadManager::GetDefaultPrioritizedAffinity()
{
	unsigned __int64 nReturn = 0;
	// build default prioitized CPU affinity
	for (unsigned int nI = 0; nI < sysInfo.dwNumberOfProcessors / 2; nI++)
	{
		nReturn |= (1ULL << nI);
	}
	return nReturn;
}

void ThreadManager::GetDefaultMatchPatterns(CString& csIncludes, CString& csExcludes)
{
	csIncludes = COREPRIO_DEFAULT_INCLUSION_PATTERN;
	csExcludes = COREPRIO_DEFAULT_EXCLUSION_PATTERN;
}
