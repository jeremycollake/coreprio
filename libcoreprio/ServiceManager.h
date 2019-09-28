#pragma once

/* !LEGACY CODE WARNING(15 + years) */

class ServiceManager
{	
	const int recheckStatusIntervalMs = 500;
public:		
	bool DoesServiceExist(const WCHAR *ptszServiceName);
	bool Stop(const WCHAR *ptszServiceName, const unsigned int nMaxWaitMs);
	bool IsServiceStopped(const WCHAR *ptszServiceName);
	bool Start(const WCHAR *ptszServiceName, const unsigned int nMaxWaitMs);
	bool IsServiceDisabled(const WCHAR *ptszServiceName);
	bool IsServiceStarted(const WCHAR *ptszServiceName);
	bool EnsureServiceIsStarted(const WCHAR* ptszServiceName, const unsigned int nMaxWaitMs);
	bool EnsureServiceIsEnabled(const WCHAR* ptszServiceName);
};
