/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*
* Console mode app, invokes core functionality in ThreadManager
*
*/
#include "pch.h"
#include "../libcoreprio/ThreadManagement.h"
#include "../libcoreprio/ServiceManager.h"
#include "../libcoreprio/productoptions.h"

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
void ShowHelp(bool bInvalid = false);

// global event to signal app shutdown
HANDLE g_hExit = NULL;

// the ThreadManager class constains core functionality (in libcoreprio)
// since single instance for app, use a global for easy access in exception handler
ThreadManager *pThreadManager = NULL;

LONG WINAPI TopLevelExceptionHandler(_In_ struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	wprintf(L"\n ! Caught unhandled exception!");
	SetEvent(g_hExit);
	// cleanup thread management
	pThreadManager->End();
	return EXCEPTION_CONTINUE_SEARCH;
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{		
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		wprintf(L"\n > Ctrl event");		
		SetEvent(g_hExit);
		return TRUE;
	default:
		return FALSE;
	}
}

void ShowHelp(bool bInvalidUse)
{
	if (true == bInvalidUse)
	{
		wprintf(L"\n ERROR: INVALID COMMAND LINE!");
		wprintf(L"\n");
	}
	wprintf(L"\n USAGE: CorePrio [-i refresh_rate_ms] [-n sw_thread_count] [-f] [-a prioritized_affinity]\n          [-l,--inclusions include_pattern] [-x,--exclusions exclude_pattern]");
	wprintf(L"\n");
	wprintf(L"\n  Defaults:");
	wprintf(L"\n            -i (rate) -> %u (ms)", COREPRIO_DEFAULT_REFRESH_RATE);
	wprintf(L"\n            -n (threads) -> half of logical core count");
	wprintf(L"\n            -a (affinity) -> first half of logical cores");
	wprintf(L"\n            -l,--inclusions -> '%s'", COREPRIO_DEFAULT_INCLUSION_PATTERN);
	wprintf(L"\n            -x,--exclusions-> '%s'", COREPRIO_DEFAULT_EXCLUSION_PATTERN);
	wprintf(L"\n");
	wprintf(L"\n  Inclusion and exclusion patterns are semicolon ';' delimited. Inclusions processed first, then exclusions.");
	wprintf(L"\n");
	wprintf(L"\n  The -f parameter enforces an experimental performance fix for Windows NUMA and disables DLM.");
	wprintf(L"\n");
	wprintf(L"\n  Example: CorePrio -i %u -n 32 -a 00000000ffffffff --inclusions * --exclusions someapp.exe;more*.exe", COREPRIO_DEFAULT_REFRESH_RATE);
	wprintf(L"\n   Runs CorePrio with at a refresh rate of %u ms, managing 32 threads, with prioritized CPU affinity of 0-31.", COREPRIO_DEFAULT_REFRESH_RATE);
	wprintf(L"\n   It includes all processes (*) and exclusions those matching 'someapp.exe' or 'more*.exe'.");
	wprintf(L"\n\n");	
}

int wmain(int argc, const wchar_t *argv[])
{
	wprintf(L"\n CorePrio (c)2019 Jeremy Collake, Bitsum LLC - https://bitsum.com");
	wprintf(L"\n build %s %hs\n", __BUILD__COREPRIO__, __DATE__);

	BOOL bApplyNUMAFix = FALSE;
	unsigned __int64 bitvecAllAffinityMask = 0;
	unsigned __int64 bitvecTargetAffinity = 0;
	DWORD nThreadsToManage = 0;
	DWORD nRefreshRateMilliseconds = COREPRIO_DEFAULT_REFRESH_RATE;
	CString csIncludes, csExcludes;

	ProductOptions prodOptions_machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
	
	// load persistent settings
		
	prodOptions_machine.get_value(REG_VALUE_PRI_AFF, bitvecTargetAffinity);
	prodOptions_machine.get_value(REG_VALUE_THREAD_COUNT, nThreadsToManage);
	prodOptions_machine.get_value(REG_VALUE_REFRESH_RATE, nRefreshRateMilliseconds);
	// for console use, force command line option to turn on NUMA fix instead of loading persistent setting
	//prodOptions_machine.get_value(REG_VALUE_APPLY_NUMA_FIX, bApplyNUMAFix);
	
	// command line options the lazy way
	for (int nI = 1; nI < argc; nI++)
	{
		if (_tcsstr(argv[nI], _T("-?")) || _tcsstr(argv[nI], _T("/?")) || _tcsstr(argv[nI], _T("-h")) || _tcsstr(argv[nI], _T("-H")))
		{			
			ShowHelp();
			return 0;
		}
		// must come before -i
		else if (wcsstr(argv[nI], _T("-l")) || wcsstr(argv[nI], _T("-L")) || wcsstr(argv[nI], _T("-inclusions")))
		{
			if (wcsstr(argv[nI], L"="))
			{
				wprintf(L"\n ERROR: Parse error on --inclusions (don't use '=')");
				ShowHelp(true);
			}
			if (nI + 1 < argc)
			{
				// must ALWAYS support '0' , or not given
				csIncludes = argv[nI + 1];
				if (csIncludes.IsEmpty())
				{
					goto inclusion_help;
				}
				verbosewprintf(L"\n + cmdline: Include pattern %s", csIncludes.GetBuffer());
				nI++;
			}
			else
			{
			inclusion_help:
				wprintf(L"\n ERROR: Parse error on --inclusions");
				ShowHelp(true);
			}
		}
		else if (wcsstr(argv[nI], _T("-x")) || wcsstr(argv[nI], _T("-X")) || wcsstr(argv[nI], _T("-exclusions")))
		{
			if (wcsstr(argv[nI], L"="))
			{
				verbosewprintf(L"\n ERROR: Parse error on --exclusions (don't use '=')");
				ShowHelp(true);
			}
			if (nI + 1 < argc)
			{
				// must ALWAYS support '0' , or not given
				csExcludes = argv[nI + 1];
				if (csExcludes.IsEmpty())
				{
					goto exclusion_help;
				}
				wprintf(L"\n + cmdline: Exclude pattern %s", csExcludes.GetBuffer());
				nI++;
			}
			else
			{
			exclusion_help:
				wprintf(L"\n ERROR: Parse error on --exclusions");
				ShowHelp(true);
			}
		}
		else if (wcsstr(argv[nI], _T("-a")) || wcsstr(argv[nI], _T("-A")))
		{
			if (wcsstr(argv[nI], L"="))
			{
				wprintf(L"\n ERROR: Parse error on -a (don't use '=')");
				ShowHelp(true);
			}
			if (nI + 1 < argc)
			{
				// must ALWAYS support '0' , or not given
				std::wstring wstrAffinity = argv[nI + 1];
				bitvecTargetAffinity = stoull(wstrAffinity, NULL, 16);
				if (bitvecTargetAffinity)
				{
					verbosewprintf(L"\n + cmdline: Priotized affinity 0x%I64x", bitvecTargetAffinity);
				}
				nI++;
			}
			else
			{	
				wprintf(L"\n ERROR: Parse error on -a");
				ShowHelp(true);
			}
		}
		else if (wcsstr(argv[nI], _T("-n")) || wcsstr(argv[nI], _T("-N")))
		{
			if (wcsstr(argv[nI], L"="))
			{
				wprintf(L"\n ERROR: Parse error on -n (don't use '=')");
				ShowHelp(true);
			}
			if (nI + 1 < argc)
			{
				// must ALWAYS support '0' , or not given
				nThreadsToManage = _ttol(argv[nI + 1]);
				if (nThreadsToManage)
				{
					verbosewprintf(L"\n + cmdline: Threads to manage %u", nThreadsToManage);
				}				
				nI++;
			}
			else
			{
				wprintf(L"\n ERROR: Parse error on -n");
				ShowHelp(true);
			}
		}	
		else if (wcsstr(argv[nI], _T("-i")) || wcsstr(argv[nI], _T("-I")))
		{
			if (wcsstr(argv[nI], L"="))
			{
				wprintf(L"\n ERROR: Parse error on -i (don't use '=')");
				ShowHelp(true);
			}
			if (nI + 1 < argc)
			{
				// must ALWAYS support '0' , or not given
				nRefreshRateMilliseconds = _ttol(argv[nI + 1]);
				if (nRefreshRateMilliseconds)
				{
					verbosewprintf(L"\n + cmdline: Refresh rate manage %u ms", nRefreshRateMilliseconds);
				}
				nI++;
			}
			else
			{
				wprintf(L"\n ERROR: Parse error on -i");
				ShowHelp(true);
			}
		}
		else if (wcsstr(argv[nI], _T("-f")) || wcsstr(argv[nI], _T("-F")))
		{
			bApplyNUMAFix = TRUE;			
		}
	}
	
	ServiceManager cServices;
	if (true == cServices.IsServiceStarted(COREPRIO_SERVICE_NAME))
	{
		wprintf(L"\n WARNING: Coreprio serice is started. Stopping it ...");
		if (false == cServices.Stop(COREPRIO_SERVICE_NAME, COREPRIO_SERVICE_WAIT_TIME_MS))
		{
			wprintf(L"\n ERROR: Could not stop Coreprio service. Can not run this utility with serice running.");
			return 1;
		}
	}

	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	g_hExit = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	SetUnhandledExceptionFilter(TopLevelExceptionHandler);

	if (SetConsoleCtrlHandler(CtrlHandler, TRUE))
	{
		wprintf(L"\n BEGINNING THREAD MANAGEMENT - PRESS CTRL+C TO SIGNAL STOP\n");
		pThreadManager = new ThreadManager(TRUE, nRefreshRateMilliseconds, nThreadsToManage, bitvecTargetAffinity, bApplyNUMAFix, csIncludes, csExcludes, LogOut::LTARGET_STDOUT);
		// begin thread management
		pThreadManager->Begin(g_hExit);
	}
	else
	{
		wprintf(L" ! ERROR: Could not register console event handler");
	}
	wprintf(L"\n");
	return 0;
}




