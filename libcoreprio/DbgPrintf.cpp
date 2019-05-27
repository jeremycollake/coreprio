#include "stdafx.h"
#include "dbgprintf.h"

// primary debug emission
void DbgPrintf(LPCTSTR fmt, ...)
{
	CString csTemp;
	va_list marker;
	va_start(marker, fmt);
	csTemp.FormatV(fmt, marker);
	va_end(marker);
	OutputDebugString(csTemp);
}
