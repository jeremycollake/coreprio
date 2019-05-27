// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <shellapi.h>
#include <crtdbg.h>
#include <WinInet.h>
#include <commctrl.h>

#include <atlstr.h>
#include <string>

#include "corepriow.h"
#include "../libcoreprio/misc.h"
#include "../libcoreprio/DbgPrintf.h"
#include "../libcoreprio/ThreadManagement.h"
#include "../libcoreprio/ServiceManager.h"

using namespace std;

