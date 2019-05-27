// this base service control code originally dervied from an ancient MSDN sample
#pragma once 

#include <windows.h> 


class CServiceBase
{
public:
	static BOOL Run(CServiceBase &service);
	CServiceBase(const WCHAR * pszServiceName,
		BOOL fCanStop = TRUE,
		BOOL fCanShutdown = TRUE,
		BOOL fCanPauseContinue = FALSE);
	virtual ~CServiceBase(void);
	void Stop();

protected:
	virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
	virtual void OnStop();
	virtual void OnPause();
	virtual void OnContinue();
	virtual void OnShutdown();
	void SetServiceStatus(DWORD dwCurrentState,
		DWORD dwWin32ExitCode = NO_ERROR,
		DWORD dwWaitHint = 0);
	void WriteEventLogEntry(const WCHAR* pszMessage, WORD wType);
	void WriteErrorLogEntry(const WCHAR* pszFunction,
		DWORD dwError = GetLastError());

private:
	const WCHAR *m_name;
	SERVICE_STATUS m_status;
	SERVICE_STATUS_HANDLE m_statusHandle;

	static void WINAPI ServiceMain(DWORD dwArgc, LPWSTR *lpszArgv);
	static void WINAPI ServiceCtrlHandler(DWORD dwCtrl);	
	void Start(DWORD dwArgc, PWSTR *pszArgv);
	void Pause();	
	void Continue();
	void Shutdown();
	
	static CServiceBase *s_service;
};