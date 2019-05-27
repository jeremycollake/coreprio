#include "pch.h"
#include <stdio.h> 
#include <windows.h> 
#include "ServiceInstaller.h" 
#include "ServiceBase.h" 
#include "CorePrioService.h" 
#include "../libcoreprio/ThreadManagement.h"
#include "../corepriow/corepriow.h"
#pragma endregion 


#define SERVICE_NAME             COREPRIO_SERVICE_NAME
#define SERVICE_DISPLAY_NAME     L"Coreprio Service" 
#define SERVICE_START_TYPE       SERVICE_AUTO_START
#define SERVICE_DEPENDENCIES     L"" 
#define SERVICE_ACCOUNT          NULL
#define SERVICE_PASSWORD         NULL 


int wmain(int argc, wchar_t *argv[])
{
	wprintf(L"\n CorePrio (c)2019 Bitsum LLC, Jeremy Collake - https://bitsum.com");
	wprintf(L"\n build %s prototype %hs\n\n", __BUILD__COREPRIO__, __DATE__);

	if ((argc > 1) && ((*argv[1] == L'-' || (*argv[1] == L'/'))))
	{
		if (_wcsicmp(L"install", argv[1] + 1) == 0)
		{
			// Install the service when the command is  
			// "-install" or "/install". 
			InstallService(
				SERVICE_NAME,               // Name of service 
				SERVICE_DISPLAY_NAME,       // Name to display 
				SERVICE_START_TYPE,         // Service start type 
				SERVICE_DEPENDENCIES,       // Dependencies 
				SERVICE_ACCOUNT,            // Service running account 
				SERVICE_PASSWORD            // Password of the account 
			);
		}
		else if (_wcsicmp(L"remove", argv[1] + 1) == 0)
		{
			// Uninstall the service when the command is  
			// "-remove" or "/remove". 
			UninstallService(SERVICE_NAME);
		}
	}
	else
	{
		wprintf(L"Parameters:\n");
		wprintf(L" -install  to install the service.\n");
		wprintf(L" -remove   to remove the service.\n");

		CCorePrioService service(SERVICE_NAME);
		if (!CCorePrioService::Run(service))
		{
			wprintf(L"Service failed to run w/err 0x%08lx\n", GetLastError());
		}
	}

	return 0;
}