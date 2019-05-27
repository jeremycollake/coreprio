#pragma once
#include <windows.h>
#include <iostream>
#include <vector>

#define DEBUG_TRACING

#if defined(DEBUG) || defined(DEBUG_TRACING)
#define MyDebugOutput OutputDebugString
#else
#define MyDebugOutput
#endif

// output to log or debug
class LogOut
{	
public:
	enum LOG_TARGET
	{
		LTARGET_DEBUG = 0,
		LTARGET_STDOUT,
		LTARGET_FILE
	};
	LOG_TARGET logTarget;
	
	LogOut();	
	void Write(LPCTSTR fmt, ...);

	void FormattedErrorOut(LPCTSTR msg);
};
