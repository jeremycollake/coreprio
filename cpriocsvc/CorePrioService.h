/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#pragma once 

#include "ServiceBase.h" 
#include "../libcoreprio/productoptions.h"
#include "../libcoreprio/DbgPrintf.h"
#include "../corepriow/corepriow.h"

using namespace std;

#define AMD_DLM_SERVICE_NAME L"AMDDynamicLocalModeService"

class CCorePrioService : public CServiceBase
{
public:

	CCorePrioService(const WCHAR * pszServiceName,
		BOOL fCanStop = TRUE,
		BOOL fCanShutdown = TRUE,
		BOOL fCanPauseContinue = FALSE);
	virtual ~CCorePrioService(void);

protected:

	virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
	virtual void OnStop();

	void ServiceWorkerThread(void);
	void ServiceWatchAMDDLMThread(void);

private:
	
	HANDLE m_hStoppedEvent;
	HANDLE m_hStoppingEvent;
};