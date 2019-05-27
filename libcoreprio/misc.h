#pragma once

#include <map>

UINT GetDlgItemTextEx(HWND hDlg, UINT nID, CString &csResult);
bool wildicmpEx(const TCHAR *wild, const TCHAR *str);
