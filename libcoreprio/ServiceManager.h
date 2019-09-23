#pragma once

/* !LEGACY CODE WARNING(15 + years) */

class ServiceManager
{	
public:		
	bool DoesServiceExist(const WCHAR *ptszServiceName);
	bool Stop(const WCHAR *ptszServiceName, const bool bWait);
	bool IsServiceStopped(const WCHAR *ptszServiceName);
	bool Start(const WCHAR *ptszServiceName, const bool bWait);
	bool IsServiceDisabled(const WCHAR *ptszServiceName);
	bool IsServiceStarted(const WCHAR *ptszServiceName);
	bool MakeSureServiceIsStarted(const WCHAR* ptszServiceName, const bool bWait);
	bool MakeSureServiceIsEnabled(const WCHAR* ptszServiceName);
};
