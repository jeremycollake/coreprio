/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#include "stdafx.h"
#include "UpdateCheckThread.h"
#include "AffinitySelectionDialog.h"
#include "../libcoreprio/TaskScheduler.h"
#include "../libcoreprio/productoptions.h"

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING] = { 0 };
WCHAR szWindowClass[MAX_LOADSTRING] = { 0 };
WCHAR szUpdateAvailableFmt[MAX_LOADSTRING] = { 0 };
WCHAR szNoUpdateAvailableFmt[MAX_LOADSTRING] = { 0 };
HWND hWndSystray = NULL;
bool gbMinimizeToTray = false;

HANDLE hUpdateIsPending = NULL;	// event signalled when an update is available on the server

ServiceManager gServiceManager;

extern CString g_csLatestVersionText;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    CorePrioMainDialog(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
	
    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_COREPRIOW, szWindowClass, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_UPDATE_AVAILABLE_FMT, szUpdateAvailableFmt, MAX_LOADSTRING);
	LoadStringW(hInstance, IDS_NO_UPDATE_AVAILABLE_FMT, szNoUpdateAvailableFmt, MAX_LOADSTRING);
	
    MyRegisterClass(hInstance);

	// command line args
	// -updateprep
	CString csCommandLine=GetCommandLine();

	CTaskScheduler TaskScheduler;
	WCHAR wszFile[MAX_PATH * 2] = { 0 };
	GetModuleFileName(NULL, wszFile, _countof(wszFile));
#define COREPRIO_STARTUP_TASK_NAME L"Coreprio System Tray"
	if (csCommandLine.Find(L"-install") != -1)
	{		
		// create startup task
		if (false == TaskScheduler.CreateStartupTask(COREPRIO_STARTUP_TASK_NAME, wszFile, L"-tray", NULL, true, true))
		{
			DEBUG_PRINT(L"ERROR: Could not create startup task. Make sure Task Scheduler service is started.");
			return 1;
		}
		return 0;
	}
	else if (csCommandLine.Find(L"-uninstall") != -1)
	{
		DEBUG_PRINT(L"Uninstalling by -uninstall");
		if (false == TaskScheduler.RemoveStartupTask(COREPRIO_STARTUP_TASK_NAME))
		{
			DEBUG_PRINT(L"ERROR removing Task Scheduler task");
			return 1;
		}
		RegDeleteKey(HKEY_CURRENT_USER, L"Software\\Coreprio");
		RegDeleteKey(HKEY_LOCAL_MACHINE, L"Software\\Coreprio");
		return 0;
	}
	else if (csCommandLine.Find(L"-updateprep") != -1 || csCommandLine.Find(L"-terminate") != -1)
	{
		DEBUG_PRINT(L"Closing corepriow and stopping service");
		// stop service and any corepriow instance
		if (gServiceManager.IsServiceStarted(COREPRIO_SERVICE_NAME))
		{
			if (false == gServiceManager.Stop(COREPRIO_SERVICE_NAME, COREPRIO_SERVICE_WAIT_TIME_MS))
			{
				DEBUG_PRINT(L"Service failed to stop");
			}
		}
		HWND hWndExisting = FindWindow(szWindowClass, szTitle);
		if (hWndExisting)
		{
			DEBUG_PRINT(L"Instructing instance to terminate");			
			// terminate existing instance
			SendMessage(hWndExisting, UWM_SYSTRAY_EXIT, 0, 0);				
			return FALSE;
		}

		return 0;
	}
	else if (csCommandLine.Find(L"-stop") != -1)
	{
		DEBUG_PRINT(L"corepriow -stio");
			
		return gServiceManager.Stop(COREPRIO_SERVICE_NAME, COREPRIO_SERVICE_WAIT_TIME_MS) ? 0 : 1;
	}
	else if (csCommandLine.Find(L"-start") != -1)
	{
		DEBUG_PRINT(L"corepriow -start");

		return gServiceManager.Start(COREPRIO_SERVICE_NAME, COREPRIO_SERVICE_WAIT_TIME_MS) ? 0 : 1;		
	}
	// command line options not exclusive	
	if (csCommandLine.Find(L"-tray") != -1)
	{
		DEBUG_PRINT(L"corepriow -tray");
		gbMinimizeToTray = true;
	}
	// we use this event to determine if any instance is running as we wait for termination
	// it doesn't necessarily get signalled (though we do), but may close and a wait be abandoned
	HANDLE hInstanceRunningEvent = CreateEvent(NULL, FALSE, FALSE, L"Global\\CoreprioRunning");

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_COREPRIOW));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }	

    return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, (LPCTSTR)IDI_COREPRIOW);
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName = (LPCTSTR)IDM_SYSTRAY_MENU;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   // make sure we are the only instance
   // if existing instance, bring to front
   HWND hWndExisting=FindWindow(szWindowClass, szTitle);
   if (hWndExisting)
   {
	   DEBUG_PRINT(L"Existing instance found, invoking settings dialog of it, then exiting new instance");
	   	// invoke the settings dialog
	   SendMessage(hWndExisting, UWM_SYSTRAY, 0, WM_LBUTTONUP);	   
	   return FALSE;
   }

   hUpdateIsPending = CreateEvent(NULL, TRUE, FALSE, NULL);

   hWndSystray = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWndSystray)
   {
      return FALSE;
   }   

   // register our system tray icon
   // NIM_ADD done in message handler
   SendMessage(hWndSystray, UWM_ADD_ICON, 0, 0);

   if (false == gbMinimizeToTray)
   {
	   SendMessage(hWndSystray, UWM_SYSTRAY, 0, WM_LBUTTONUP);
   }

   //ShowWindow(hWnd, nCmdShow);
   //UpdateWindow(hWnd);

   return TRUE;
}

// lpv = HWND parent
DWORD WINAPI CorePrioDialogThread(LPVOID lpv)
{
	DEBUG_PRINT(L"CorePrioDialogThread");
	HWND * phWndParent= (HWND *)lpv;	
	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_COREPRIO), NULL, &CorePrioMainDialog, 0);
	return 0;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
#define COREPRIO_UPDATE_RESULT_TIMER_ID 0x8444
#define COREPRIO_CHECK_SERVICE_STATE 0x8445
#define COREPRIO_UPDATE_CHECK_PERIODIC_TIMER_ID 0x8446
#define COREPRIO_SERVICE_CHECK_TIMER_ID 0x8447

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{	
	NOTIFYICONDATA ndata;
	POINT pt;
	DWORD dwTID;
	CString csTemp;
	static HMENU hmenu = NULL;
	static HMENU hpopup = NULL;
	static UINT uTaskbarRestartSysTrayWindowMsg=0;
	static HANDLE hUpdateCheckDoneEvent = NULL;
	static HANDLE hUpdateCheckThread = NULL;	
	static bool bNotificationWaiting_ShowChanges = false;
	static bool bNotificationWaiting_DoUpdate = false;
	static bool bLastStartedValue = false;
	static ProductOptions prodOptions_user(HKEY_CURRENT_USER, PRODUCT_NAME);
	bool bCurrentStartedValue;

    switch (message)
    {
	case WM_CREATE:
		
		// register for explorer restart notifications
		if (0 == uTaskbarRestartSysTrayWindowMsg)
		{
			uTaskbarRestartSysTrayWindowMsg = RegisterWindowMessage(_T("TaskbarCreated"));
		}
		_ASSERT(uTaskbarRestartSysTrayWindowMsg);

		if (NULL == hmenu)
		{
			// preload systray menu, as we'll just maintain a single persistant instance of the backing resource	
			hmenu = LoadMenu(hInst, MAKEINTRESOURCE(IDM_SYSTRAY_MENU));
			hpopup = GetSubMenu(hmenu, 0);			
		}
		_ASSERT(hmenu && hpopup);

		if (NULL == hUpdateCheckDoneEvent)
		{
			// create update check thread if in stand-alone mode (system tray)		
			hUpdateCheckDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		}		
		
		SetTimer(hWnd, COREPRIO_SERVICE_CHECK_TIMER_ID, 2500, NULL);

		SetTimer(hWnd, COREPRIO_UPDATE_CHECK_PERIODIC_TIMER_ID, 1000 * 60 * 60 * 4, NULL);
		PostMessage(hWnd, WM_TIMER, COREPRIO_UPDATE_CHECK_PERIODIC_TIMER_ID, 0);

		// if we finished an update, show a message
		if(true==prodOptions_user[COREPRIO_UPDATE_FINISHED_REGVAL])
		{		
			PostMessage(hWnd, UWM_SHOW_POST_UPDATE_NOTIFICATION, 0, 0);
			prodOptions_user.set_value(COREPRIO_UPDATE_FINISHED_REGVAL, false);
		}				

		break;
	case UWM_INITIATE_UPDATE_CHECK:
		ResetEvent(hUpdateIsPending);
		ResetEvent(hUpdateCheckDoneEvent);
		SetTimer(hWnd, COREPRIO_UPDATE_RESULT_TIMER_ID, 1000, NULL);
		hUpdateCheckThread = CreateThread(NULL, 0, &UpdateCheckThread, hUpdateCheckDoneEvent, 0, &dwTID);
		break;
	case WM_TIMER:
		// timer 1 - check for update return. 
		switch (wParam)
		{
		case COREPRIO_SERVICE_CHECK_TIMER_ID:
			// todo: switch to polling from windows event source
			bCurrentStartedValue=gServiceManager.IsServiceStarted(COREPRIO_SERVICE_NAME);
					
			if (bCurrentStartedValue != bLastStartedValue)
			{
				PostMessage(hWnd, UWM_SERVICE_STATE_CHANGE, bCurrentStartedValue ? 1 : 0, 0);
				bLastStartedValue = bCurrentStartedValue;				
			}
			break;
		case COREPRIO_UPDATE_CHECK_PERIODIC_TIMER_ID:
			PostMessage(hWnd, UWM_INITIATE_UPDATE_CHECK, 0, 0);
			break;
		case COREPRIO_UPDATE_RESULT_TIMER_ID:
			if (hUpdateCheckDoneEvent
				&& WaitForSingleObject(hUpdateCheckDoneEvent, 0) == WAIT_OBJECT_0)
			{
				DEBUG_PRINT(L"Update check finished. Handling results.");
				
				// now show or hide the 'One click update' option, which will be available only to licensed users					
				if (TextVersionToULONG(CURRENT_VERSION) < TextVersionToULONG(g_csLatestVersionText))
				{
					DEBUG_PRINT(L"Update available. Signaling.");
					csTemp.Format(szUpdateAvailableFmt, g_csLatestVersionText);
					SetEvent(hUpdateIsPending);
					// notify user of update available (also shown in dialog)					
					PostMessage(hWnd, UWM_SHOW_UPDATE_AVAILABLE_NOTIFICATION, 0, 0);
				}
				else
				{
					// also set online version to current version so there is no display discrepency if we want to throttle an update
					csTemp.Format(szNoUpdateAvailableFmt, CURRENT_VERSION);
					ResetEvent(hUpdateIsPending);
				}
				ResetEvent(hUpdateCheckDoneEvent);
				// stop checking for update check events - must restart if we initiate new update check
				KillTimer(hWnd, COREPRIO_UPDATE_RESULT_TIMER_ID);
			}
			break;
		}
		break;
    
    case WM_DESTROY:
		// remove our system try icon
		memset(&ndata, 0, sizeof(NOTIFYICONDATA));
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = COREPRIO_SYSTEM_TRAY_UNIQUE_ID;
		ndata.uCallbackMessage = UWM_SYSTRAY;
		Shell_NotifyIcon(NIM_DELETE, &ndata);

        PostQuitMessage(0);
        break;

	case UWM_ADD_ICON:
		DEBUG_PRINT(L"UWM_ADD_ICON");
		memset(&ndata, 0, sizeof(ndata));
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = COREPRIO_SYSTEM_TRAY_UNIQUE_ID;
		ndata.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		wcsncpy_s(ndata.szTip, _countof(ndata.szTip), L"Coreprio ", _TRUNCATE);
		wcscat_s(ndata.szTip, _countof(ndata.szTip), CURRENT_VERSION_FRIENDLY);		
		ndata.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_COREPRIOW)); ;
		ndata.uTimeout = 10000; // just to have a default timeout
		ndata.uCallbackMessage = UWM_SYSTRAY;
		Shell_NotifyIcon(NIM_ADD, &ndata);

		// set the initial tooltip
		bCurrentStartedValue = gServiceManager.IsServiceStarted(COREPRIO_SERVICE_NAME);
		PostMessage(hWnd, UWM_SERVICE_STATE_CHANGE, bCurrentStartedValue ? 1 : 0, 0);
		bLastStartedValue = bCurrentStartedValue;

		break;
	case UWM_SHOW_POST_UPDATE_NOTIFICATION:
		DEBUG_PRINT(L"UWM_SHOW_POST_UPDATE_NOTIFICATION");
		memset(&ndata, 0, sizeof(ndata));
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = COREPRIO_SYSTEM_TRAY_UNIQUE_ID;
		ndata.uFlags = NIF_INFO;
		ndata.uTimeout = 10000; // just to have a default timeout		
		wcscpy_s(ndata.szInfoTitle, _countof(ndata.szInfoTitle) - 1, L"Coreprio Updated");		
		wsprintf(ndata.szInfo, L"Newly installed version is %s. Click here for the change log.", CURRENT_VERSION);
		Shell_NotifyIcon(NIM_MODIFY, &ndata);
		bNotificationWaiting_ShowChanges = true;
		break;
	case UWM_SHOW_UPDATE_AVAILABLE_NOTIFICATION:
		DEBUG_PRINT(L"UWM_SHOW_UPDATE_AVAILABLE_NOTIFICATION");
		memset(&ndata, 0, sizeof(ndata));
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = COREPRIO_SYSTEM_TRAY_UNIQUE_ID;
		ndata.uFlags = NIF_INFO;
		ndata.uTimeout = 10000;
		wcscpy_s(ndata.szInfoTitle, _countof(ndata.szInfoTitle) - 1, L"Update for Coreprio is available");
		wsprintf(ndata.szInfo, L"Click here for a single-click product update.", CURRENT_VERSION);
		Shell_NotifyIcon(NIM_MODIFY, &ndata);
		bNotificationWaiting_DoUpdate = true;
		break;
	case UWM_SERVICE_STATE_CHANGE:
		DEBUG_PRINT(L"UWM_SERVICE_STATE_CHANGE");
		memset(&ndata, 0, sizeof(ndata));
		ndata.cbSize = sizeof(NOTIFYICONDATA);
		ndata.hWnd = hWnd;
		ndata.uID = COREPRIO_SYSTEM_TRAY_UNIQUE_ID;
		ndata.uFlags = NIF_TIP;
		wcsncpy_s(ndata.szTip, _countof(ndata.szTip), L"Coreprio ", _TRUNCATE);
		wcscat_s(ndata.szTip, _countof(ndata.szTip), CURRENT_VERSION_FRIENDLY);
		wcscat_s(ndata.szTip, _countof(ndata.szTip), L"\n");
		if (1 == wParam)
		{
			wcscat_s(ndata.szTip, _countof(ndata.szTip), L"Service is RUNNING.");
		}
		else
		{
			wcscat_s(ndata.szTip, _countof(ndata.szTip), L"Service is stopped.");
		}		
		ndata.uTimeout = 10000; // just to have a default timeout
		ndata.uCallbackMessage = UWM_SYSTRAY;
		Shell_NotifyIcon(NIM_MODIFY, &ndata);
		break;
	case UWM_SYSTRAY_EXIT:
		DestroyWindow(hWnd);
		break;
	case UWM_SYSTRAY:
		switch (lParam)
		{
		case NIN_BALLOONHIDE:
			DEBUG_PRINT(L"NIN_BALLOONHIDE");
			bNotificationWaiting_ShowChanges = false;
			bNotificationWaiting_DoUpdate = false;
			break;
		case NIN_BALLOONTIMEOUT:
			DEBUG_PRINT(L"NIN_BALLOONTIMEOUT");
			bNotificationWaiting_ShowChanges = false;
			bNotificationWaiting_DoUpdate = false;
			break;
		case NIN_BALLOONUSERCLICK:  // XP and above
			// clicked the balloon we last showed ... 	
			DEBUG_PRINT(L"NIM_BALLOONUSERCLICK");
			if (true == bNotificationWaiting_ShowChanges)
			{
				ShellExecute(NULL, NULL, PRODUCT_CHANGES_URL, NULL, NULL, SW_SHOW);
				bNotificationWaiting_ShowChanges = false;
			}
			else if (true == bNotificationWaiting_DoUpdate)
			{
				PerformAutomaticUpdate();
				bNotificationWaiting_DoUpdate = false;
			}
			break;
		case WM_LBUTTONDBLCLK:
			break;		// do not remove
		case WM_LBUTTONUP:
			
			CloseHandle(
				CreateThread(NULL, 0, CorePrioDialogThread, (LPVOID)hWnd, 0, &dwTID));

			break;
		case WM_RBUTTONUP:
			
			// update menu with service running state
			if (gServiceManager.IsServiceStarted(COREPRIO_SERVICE_NAME))
			{
				EnableMenuItem(hpopup, ID_SYSTRAY_STARTSERVICE, MF_GRAYED | MF_BYCOMMAND | MF_DISABLED);				
				EnableMenuItem(hpopup, ID_SYSTRAY_STOPSERVICE, MF_BYCOMMAND | MF_ENABLED);
			}
			else
			{
				EnableMenuItem(hpopup, ID_SYSTRAY_STOPSERVICE, MF_GRAYED | MF_BYCOMMAND | MF_DISABLED);
				EnableMenuItem(hpopup, ID_SYSTRAY_STARTSERVICE, MF_BYCOMMAND | MF_ENABLED);				
			}
			
			// see https://msdn.microsoft.com/en-us/library/windows/desktop/ms648002(v=vs.85).aspx
			// foreground window must be first set to the systray window, else the menu won't disappear
			SetForegroundWindow(hWnd);			
			GetCursorPos(&pt);
			int nTrackRes;
			nTrackRes = (unsigned long)TrackPopupMenu(hpopup,
				TPM_RETURNCMD |
				TPM_RIGHTBUTTON,
				pt.x, pt.y,
				0,
				hWnd,
				NULL);

			// see https://msdn.microsoft.com/en-us/library/windows/desktop/ms648002(v=vs.85).aspx
			SendMessage(hWnd, WM_NULL, 0, 0);			
						
			switch (nTrackRes)
			{
			case ID_SYSTRAY_OPEN:
				CloseHandle(
					CreateThread(NULL, 0, CorePrioDialogThread, (LPVOID)hWnd, 0, &dwTID));
				break;
			case ID_SYSTRAY_EXIT:
				DestroyWindow(hWnd);
				break;
			case ID_SYSTRAY_STARTSERVICE:
				gServiceManager.Start(COREPRIO_SERVICE_NAME, true);				
				break;
			case ID_SYSTRAY_STOPSERVICE:
				gServiceManager.Stop(COREPRIO_SERVICE_NAME, true);
				break;
			}

			return 0;
		}
		break;
    default:
		if (message == uTaskbarRestartSysTrayWindowMsg)
		{
			DEBUG_PRINT(L"TaskbarCreateMessage");			
			PostMessage(hWnd, UWM_ADD_ICON, 0, 0);
		}
		else
		{
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
    }
    return 0;
}

// Message handler for main dialog
// IMPORTANT - Do not invoke 2 instances of the dialog (per process), static variables may make it unsafe
//			   Must prevent dual instances in calling code.
INT_PTR CALLBACK CorePrioMainDialog(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{	
	UNREFERENCED_PARAMETER(lParam);
	HICON hDialogIconSmall, hDialogIconBig;
	CString csTemp;
	CString csInclusions, csExclusions;
	BOOL bEnableDLM = FALSE;
	BOOL bApplyNUMAFix = FALSE;
	WCHAR wszTemp[256] = { 0 };
	WCHAR wszAffinity[128] = { 0 };
	ULONGLONG bitvecAffinity = 0;
	DWORD nThreadCount = 0;
	DWORD nRefreshRate = 0;
	static ThreadManager ThreadManager;
	static ProductOptions prodOptions_user(HKEY_CURRENT_USER, PRODUCT_NAME);
	static ProductOptions prodOptions_machine(HKEY_LOCAL_MACHINE, PRODUCT_NAME);
	static HANDLE hUpdateCheckThread;
	static HANDLE hUpdateCheckDoneEvent = NULL;	
	static HFONT hServiceStateFont = NULL;
	
	switch (message)
	{
	case WM_DESTROY:
		DEBUG_PRINT(L"WM_DESTROY main dialog");
		if (hServiceStateFont)
		{
			DeleteObject(hServiceStateFont);
			hServiceStateFont = NULL;
		}
		break;
	case WM_INITDIALOG:
		DEBUG_PRINT(L"WM_INITDIALOG");
		hDialogIconSmall = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_COREPRIOW), IMAGE_ICON, 16, 16, 0);
		hDialogIconBig = (HICON)LoadImage(hInst, MAKEINTRESOURCE(IDI_COREPRIOW), IMAGE_ICON, 32, 32, 0);
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hDialogIconSmall);
		SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hDialogIconBig);

		// create update check thread if in stand-alone mode (system tray)		
		_ASSERT(hUpdateIsPending);
		if (WaitForSingleObject(hUpdateIsPending, 0) == WAIT_OBJECT_0)
		{
			csTemp.Format(szUpdateAvailableFmt, g_csLatestVersionText);
			SetWindowText(GetDlgItem(hDlg, IDC_SYSLINK_VISIT_BITSUM), csTemp);
		}
		else
		{
			// also set online version to current version so there is no display discrepency if we want to throttle an update
			csTemp.Format(szNoUpdateAvailableFmt, CURRENT_VERSION);
			SetWindowText(GetDlgItem(hDlg, IDC_SYSLINK_VISIT_BITSUM), csTemp);
		}

		// fill settings
		bEnableDLM = prodOptions_machine[REG_VALUE_ENABLE_DLM];				
		CheckDlgButton(hDlg, IDC_CHECK_ENABLE_DLM, bEnableDLM ? BST_CHECKED:BST_UNCHECKED);
		DEBUG_PRINT(L"DLM is %s", bEnableDLM ? L"ENABLED" : L"DISABLED");
		
		bApplyNUMAFix = prodOptions_machine[REG_VALUE_APPLY_NUMA_FIX];				
		CheckDlgButton(hDlg, IDC_CHECK_NUMA_DISASC, bApplyNUMAFix ? BST_CHECKED : BST_UNCHECKED);
		DEBUG_PRINT(L"NUMA fix is %s", bApplyNUMAFix ? L"ENABLED" : L"DISABLED");

		if (false == prodOptions_machine.get_value(REG_VALUE_PRI_AFF, bitvecAffinity))
		{
			bitvecAffinity = ThreadManager.GetDefaultPrioritizedAffinity();
		}
		_stprintf_s(wszAffinity, L"%016I64x", bitvecAffinity);
		SetDlgItemText(hDlg, IDC_EDIT_AFFINITY, wszAffinity);

		if (false == prodOptions_machine.get_value(REG_VALUE_THREAD_COUNT, nThreadCount))
		{
			nThreadCount = ThreadManager.GetDefaultThreadsToManage();
		}
		SetDlgItemInt(hDlg, IDC_EDIT_THREAD_COUNT, nThreadCount, FALSE);

		if (false == prodOptions_machine.get_value(REG_VALUE_REFRESH_RATE, nRefreshRate))
		{
			nRefreshRate = ThreadManager.GetDefaultRefreshRate();
		}		
		SetDlgItemInt(hDlg, IDC_EDIT_REFRESH_RATE, nRefreshRate, FALSE);
		
		if (false == prodOptions_machine.get_value(REG_VALUE_INCLUSIONS, csInclusions))
		{
			ThreadManager.GetDefaultMatchPatterns(csInclusions, csExclusions);
		}
		SetDlgItemText(hDlg, IDC_EDIT_INCLUDES, csInclusions);
		
		if (false == prodOptions_machine.get_value(REG_VALUE_EXCLUSIONS, csExclusions))
		{
			ThreadManager.GetDefaultMatchPatterns(csInclusions, csExclusions);
		}
		SetDlgItemText(hDlg, IDC_EDIT_EXCLUDES, csExclusions);
		
		NONCLIENTMETRICS nonclientMetrics;
		LOGFONT logFontOriginal, logFontNew;		
		memset(&nonclientMetrics, 0, sizeof(nonclientMetrics));
		nonclientMetrics.cbSize = sizeof(NONCLIENTMETRICS);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(nonclientMetrics), &nonclientMetrics, 0);
		memcpy_s(&logFontOriginal, sizeof(logFontOriginal), &nonclientMetrics.lfMessageFont, sizeof(nonclientMetrics.lfMessageFont));

		memcpy_s(&logFontNew, sizeof(logFontNew), &logFontOriginal, sizeof(logFontOriginal));
		logFontNew.lfWeight = FW_BOLD;
		hServiceStateFont = CreateFontIndirect(&logFontNew);
		if (hServiceStateFont)
		{
			SendMessage(GetDlgItem(hDlg, IDC_SYSLINK_SERVICE_STATE), WM_SETFONT, (WPARAM)hServiceStateFont, MAKELPARAM(TRUE, 0));			
			SendMessage(GetDlgItem(hDlg, IDC_STATIC_SERVICE_MUST_BE_STOPPED), WM_SETFONT, (WPARAM)hServiceStateFont, MAKELPARAM(TRUE, 0));
		}		

		SetTimer(hDlg, COREPRIO_UPDATE_RESULT_TIMER_ID, 1000, NULL);
		SetTimer(hDlg, COREPRIO_CHECK_SERVICE_STATE, 2500, NULL);

		PostMessage(hDlg, WM_TIMER, COREPRIO_CHECK_SERVICE_STATE, 0);
		
		BringWindowToTop(hDlg);		
		SetForegroundWindow(hDlg);

		return (INT_PTR)TRUE;

	case WM_TIMER:
		switch (wParam)
		{
		case COREPRIO_CHECK_SERVICE_STATE:
			if (gServiceManager.IsServiceStarted(COREPRIO_SERVICE_NAME))
			{
				SetDlgItemText(hDlg, IDC_SYSLINK_SERVICE_STATE, L"Service running. <a>STOP</a>");							
				EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SELECT_PRIOTIZED_CPU_AFFINITY), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_REFRESH_RATE), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_THREAD_COUNT), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_AFFINITY), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULTS), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_ENABLE_DLM), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NUMA_DISASC), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_INCLUDES), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_EXCLUDES), FALSE);
				ShowWindow(GetDlgItem(hDlg, IDC_STATIC_SERVICE_MUST_BE_STOPPED), SW_NORMAL);
			}
			else
			{
				SetDlgItemText(hDlg, IDC_SYSLINK_SERVICE_STATE, L"Service stopped. <a>START</a>");
				EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_SELECT_PRIOTIZED_CPU_AFFINITY), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_REFRESH_RATE), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_THREAD_COUNT), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_AFFINITY), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_DEFAULTS), TRUE);				
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_ENABLE_DLM), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NUMA_DISASC), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_INCLUDES), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_EXCLUDES), TRUE);
				ShowWindow(GetDlgItem(hDlg, IDC_STATIC_SERVICE_MUST_BE_STOPPED), SW_HIDE);
			}
			break;
		case COREPRIO_UPDATE_RESULT_TIMER_ID:
			_ASSERT(hUpdateIsPending);
			if (WaitForSingleObject(hUpdateIsPending, 0) == WAIT_OBJECT_0)
			{
				csTemp.Format(szUpdateAvailableFmt, g_csLatestVersionText);
				SetWindowText(GetDlgItem(hDlg, IDC_SYSLINK_VISIT_BITSUM), csTemp);
			}
			else
			{
				// also set online version to current version so there is no display discrepency if we want to throttle an update
				csTemp.Format(szNoUpdateAvailableFmt, CURRENT_VERSION);
				SetWindowText(GetDlgItem(hDlg, IDC_SYSLINK_VISIT_BITSUM), csTemp);
			}
			KillTimer(hDlg, COREPRIO_UPDATE_RESULT_TIMER_ID);
			break;
		}
		break;

	case WM_COMMAND:		
		switch (LOWORD(wParam))
		{
		case IDC_CHECK_ENABLE_DLM:
			if (IsDlgButtonChecked(hDlg, IDC_CHECK_ENABLE_DLM)&BST_CHECKED)
			{
				bEnableDLM = TRUE;
			}
			else
			{
				bEnableDLM = FALSE;
			}
			if (bEnableDLM)
			{
				CheckDlgButton(hDlg, IDC_CHECK_ENABLE_DLM, BST_CHECKED);				
			}
			else
			{
				CheckDlgButton(hDlg, IDC_CHECK_ENABLE_DLM, BST_UNCHECKED);
			}
			break;
		case IDC_CHECK_NUMA_DISASC:
			
			break;
		case IDC_DEFAULTS:
			bitvecAffinity=ThreadManager.GetDefaultPrioritizedAffinity();
			_stprintf_s(wszAffinity, L"%016I64x", bitvecAffinity);
			SetDlgItemText(hDlg, IDC_EDIT_AFFINITY, wszAffinity);
			
			nThreadCount = ThreadManager.GetDefaultThreadsToManage();
			SetDlgItemInt(hDlg, IDC_EDIT_THREAD_COUNT, nThreadCount, FALSE);
			
			nRefreshRate = ThreadManager.GetDefaultRefreshRate();
			SetDlgItemInt(hDlg, IDC_EDIT_REFRESH_RATE, nRefreshRate, FALSE);

			ThreadManager.GetDefaultMatchPatterns(csInclusions, csExclusions);
			SetDlgItemText(hDlg, IDC_EDIT_INCLUDES, csInclusions);
			SetDlgItemText(hDlg, IDC_EDIT_EXCLUDES, csExclusions);
			
			break;
		case IDC_BUTTON_SELECT_PRIOTIZED_CPU_AFFINITY:
			wszAffinity[0] = 0;
			bitvecAffinity = 0;
			if (GetDlgItemText(hDlg, IDC_EDIT_AFFINITY, wszAffinity, _countof(wszAffinity)) && wszAffinity[0])
			{
				bitvecAffinity = stoull(wszAffinity, NULL, 16);
			}
			if (0 != DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_AFFINITY_DIALOG), hDlg, &SelectAffinityDialogProc, (LPARAM)&bitvecAffinity))
			{
				_stprintf_s(wszAffinity, L"%016I64x", bitvecAffinity);
				SetDlgItemText(hDlg, IDC_EDIT_AFFINITY, wszAffinity);
			}
			break;
		case IDC_BUTTON_SAVE:
			// save parameters
			// priotized affinity
			wszAffinity[0] = 0;
			GetDlgItemText(hDlg, IDC_EDIT_AFFINITY, wszAffinity, _countof(wszAffinity));
			if (wszAffinity[0])
			{
				bitvecAffinity = stoull(wszAffinity, NULL, 16);
				DEBUG_PRINT(L"Storing new prioritized affinity of %I64x", bitvecAffinity);
				prodOptions_machine.set_value(REG_VALUE_PRI_AFF, bitvecAffinity);				
			}
			// thread count
			nThreadCount = GetDlgItemInt(hDlg, IDC_EDIT_THREAD_COUNT, NULL, FALSE);
			if (nThreadCount)
			{
				DEBUG_PRINT(L"Storing new prioritized thread count of %u", nThreadCount);
				prodOptions_machine.set_value(REG_VALUE_THREAD_COUNT, nThreadCount);
			}
			// refresh rate
			nRefreshRate = GetDlgItemInt(hDlg, IDC_EDIT_REFRESH_RATE, NULL, FALSE);
			if (nRefreshRate)
			{
				DEBUG_PRINT(L"Storing new refresh rate %u", nRefreshRate);				
				prodOptions_machine.set_value(REG_VALUE_REFRESH_RATE, nRefreshRate);
			}

			if (IsDlgButtonChecked(hDlg, IDC_CHECK_ENABLE_DLM)==BST_CHECKED)
			{
				bEnableDLM = TRUE;
			}
			else
			{
				bEnableDLM = FALSE;
			}
			prodOptions_machine.set_value(REG_VALUE_ENABLE_DLM, bEnableDLM);
			
			if (IsDlgButtonChecked(hDlg, IDC_CHECK_NUMA_DISASC)==BST_CHECKED)
			{
				bApplyNUMAFix = TRUE;
			}
			else
			{
				bApplyNUMAFix = FALSE;
			}
			prodOptions_machine.set_value(REG_VALUE_APPLY_NUMA_FIX, bApplyNUMAFix);

			GetDlgItemTextEx(hDlg, IDC_EDIT_INCLUDES, csInclusions);
			GetDlgItemTextEx(hDlg, IDC_EDIT_EXCLUDES, csExclusions);			
			prodOptions_machine.set_value(REG_VALUE_INCLUSIONS, csInclusions);
			prodOptions_machine.set_value(REG_VALUE_EXCLUSIONS, csExclusions);

			break;
		case IDOK:			
			// save any settings
			SendMessage(hDlg, WM_COMMAND, IDC_BUTTON_SAVE, 0);
			// fall-through
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			break;
		}
		break;
	case WM_NOTIFY:
		// if lParam is 0, invalid WM_NOTIFY message
		if (NULL == lParam || IsBadReadPtr((LPVOID)lParam,sizeof(NMHDR)))
		{
			break;
		}
		switch (((LPNMHDR)lParam)->idFrom)
		{
		case IDC_SYSLINK_SERVICE_STATE:			
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_RETURN:
			case NM_CLICK: // fall through
				DEBUG_PRINT(L"IDC_SYSLINK_SERVICE_STATE");
				if (gServiceManager.IsServiceStarted(COREPRIO_SERVICE_NAME))
				{
					DEBUG_PRINT(L"Service is started, attempting stop");
					if(false==gServiceManager.Stop(COREPRIO_SERVICE_NAME, true))
					{
						MessageBox(hDlg, L"Coreprio service could not stop.", PRODUCT_NAME, MB_ICONSTOP);
					}
				}
				else
				{
					DEBUG_PRINT(L"Service is stopped, attempting start");
					SendMessage(hDlg, WM_COMMAND, IDC_BUTTON_SAVE, 0);
					if(false==gServiceManager.Start(COREPRIO_SERVICE_NAME, true))
					{
						MessageBox(hDlg, L"Coreprio service could not start.", PRODUCT_NAME, MB_ICONSTOP);
					}
				}				
				//PostMessage(hDlg, WM_TIMER, COREPRIO_CHECK_SERVICE_STATE, 0);
				break;
			default:
				break;
			}
			break;
		case IDC_SYSLINK_VISIT_BITSUM:			
			switch (((LPNMHDR)lParam)->code)
			{
			case NM_RETURN:
			case NM_CLICK: // fall through
				DEBUG_PRINT(L"IDC_SYSLINK_VISIT_BITSUM");
				if (WaitForSingleObject(hUpdateIsPending, 0) == WAIT_OBJECT_0)
				{
					PerformAutomaticUpdate();
					// simply go to web site until tested
					//ShellExecute(NULL, NULL, PRODUCT_URL, 0, NULL, SW_SHOW);
				}
				else
				{
					ShellExecute(NULL, NULL, PRODUCT_URL, 0, NULL, SW_SHOW);
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
