/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*
*/
#pragma once

#include "resource.h"
#include "../version.h"

//#define BETA

#define PRODUCT_NAME L"Coreprio"
#define __BUILD__COREPRIO__ CURRENT_VERSION
#define CURRENT_VERSION_FRIENDLY CURRENT_VERSION
#define PRODUCT_URL L"https://bitsum.com/portfolio/coreprio"
#define PRODUCT_CHANGES_URL L"https://bitsum.com/portfolio/coreprio/#changes"

#ifndef BETA
#define INSTALLER_DOWNLOAD_URL L"https://update.bitsum.com/files/corepriosetup.exe"
#else
#define INSTALLER_DOWNLOAD_URL L"https://update.bitsum.com/files/beta/corepriosetup.exe"
#endif
#define INSTALLER_FILENAME L"corepriosetup.exe"

#define PRODUCT_REGISTRY_KEY L"Software\\Coreprio"

#define MAX_LOADSTRING 384

#define COREPRIO_SYSTEM_TRAY_UNIQUE_ID 0x1213
#define UWM_SYSTRAY WM_USER+1
#define UWM_SYSTRAY_EXIT WM_USER+2
#define UWM_ADD_ICON WM_USER+3
#define UWM_SHOW_POST_UPDATE_NOTIFICATION WM_USER+4
#define UWM_SHOW_UPDATE_AVAILABLE_NOTIFICATION WM_USER+5
#define UWM_INITIATE_UPDATE_CHECK WM_USER+6
#define UWM_SERVICE_STATE_CHANGE WM_USER+7

#ifndef DEBUG_PRINT
#define DEBUG_PRINT(LPCTSTR, ...) DbgPrintf(LPCTSTR, __VA_ARGS__)
#endif

#define COREPRIO_UPDATE_FINISHED_REGVAL L"UpdateFinished"
#define UPDATE_CHECK_REG_VALUE_NAME L"UpdateChecksDisabled"

#define REG_VALUE_ENABLE_DLM L"EnableDLM"
#define REG_VALUE_APPLY_NUMA_FIX L"ApplyNUMAFix"
#define REG_VALUE_PRI_AFF L"PriotizedAffinity"
#define REG_VALUE_THREAD_COUNT L"ThreadCount"
#define REG_VALUE_REFRESH_RATE L"RefreshRate"
#define REG_VALUE_INCLUSIONS L"Inclusions"
#define REG_VALUE_EXCLUSIONS L"Exclusions"

#define COREPRIO_SERVICE_NAME L"cpriosvc"

#define COREPRIO_SERVICE_WAIT_TIME_MS (10*1000)