/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#include "stdafx.h"
#include "AffinitySelectionDialog.h"

void SetCPUAffinityInDialog(HWND hDlg, ULONGLONG nAffinityMask)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	unsigned int nA = 0;
	for (nA = 0; nA < MAX_GUI_SUPPORTED_CPUS && nA < sysInfo.dwNumberOfProcessors; nA++)
	{
		EnableWindow(GetDlgItem(hDlg, IDC_CPU0 + nA), TRUE);
		if (nAffinityMask & (1ULL << nA))
		{
			CheckDlgButton(hDlg, IDC_CPU0 + nA, BST_CHECKED);
		}
	}
}

void ClearSpecificCPUAffinityInDialog(HWND hDlg, ULONGLONG nAffinityMask)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	unsigned int nA = 0;
	for (nA = 0; nA < MAX_GUI_SUPPORTED_CPUS && nA < sysInfo.dwNumberOfProcessors; nA++)
	{
		if (nAffinityMask & (1ULL << nA))
		{
			CheckDlgButton(hDlg, IDC_CPU0 + nA, BST_UNCHECKED);
		}
	}
}

ULONGLONG GetCPUAffinityFromDialog(HWND hDlg)
{
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	ULONGLONG nAffinityMask = 0;
	unsigned int nA = 0;
	for (nA = 0; nA < MAX_GUI_SUPPORTED_CPUS && nA < sysInfo.dwNumberOfProcessors; nA++)
	{
		if (IsDlgButtonChecked(hDlg, IDC_CPU0 + nA) == BST_CHECKED)
		{
			nAffinityMask |= (1ULL << nA);
		}
	}
	return nAffinityMask;
}

void SyncNumaNodeSelectionWithCPUs(HWND hDlg)
{
	ULONG nHighestNUMANode = 0;

	if (FALSE == GetNumaHighestNodeNumber(&nHighestNUMANode))
	{
		nHighestNUMANode = 0;
	}

	// iterate through NUMA nodes and select appropriate CPUs
	for (unsigned int nI = 0; nI < MAX_NUMA_NODES && nI <= nHighestNUMANode; nI++)
	{
		ULONGLONG nAffinityMask = 0;
		UCHAR nNode = nI & 255;
		if (TRUE == GetNumaNodeProcessorMask(nNode, &nAffinityMask))
		{
			if (IsDlgButtonChecked(hDlg, IDC_NUMA_0 + nI) == BST_CHECKED)
			{
				SetCPUAffinityInDialog(hDlg, nAffinityMask);
			}
			else
			{
				ClearSpecificCPUAffinityInDialog(hDlg, nAffinityMask);
			}
		}
	}
}

void ClearNumaNodeCheckboxes(HWND hDlg)
{
	ULONG nHighestNUMANode = 0;

	if (FALSE == GetNumaHighestNodeNumber(&nHighestNUMANode))
	{
		nHighestNUMANode = 0;
	}
	// iterate through NUMA nodes and select appropriate CPUs
	for (unsigned int nI = 0; nI < MAX_NUMA_NODES && nI <= nHighestNUMANode; nI++)
	{
		CheckDlgButton(hDlg, IDC_NUMA_0 + nI, BST_UNCHECKED);
	}
}

void InitializeNumaNodeCheckboxes(HWND hDlg)
{
	ULONG nHighestNUMANode = 0;

	if (FALSE == GetNumaHighestNodeNumber(&nHighestNUMANode))
	{
		nHighestNUMANode = 0;
	}
	for (unsigned int nA = 0; nA < MAX_GUI_SUPPORTED_CPUS && nA <= nHighestNUMANode; nA++)
	{
		EnableWindow(GetDlgItem(hDlg, IDC_NUMA_0 + nA), TRUE);
	}
}

//
// Affinity selection dialog
//
// In:
//  Affinity mask pointed to by lParam
// Out:
//  Selected affinity mask pointed to by lParam
// Returns:
//  TRUE if selected (and output affinity filled)
//  FALSE if canceled
//
INT_PTR CALLBACK SelectAffinityDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	unsigned __int64 dwAffinity;
	unsigned __int64 *pInOut = NULL;
	unsigned int nI;
	static bool g_bAffinityDialogChangesMade = false;
	switch (message)
	{
	case WM_INITDIALOG:		
		SetWindowLongPtr(hDlg, GWLP_USERDATA, lParam);
		pInOut = (unsigned __int64 *)lParam;
		if (pInOut)
		{
			dwAffinity = *pInOut;
		}

		SetCPUAffinityInDialog(hDlg, dwAffinity);
		InitializeNumaNodeCheckboxes(hDlg);
		return TRUE;
	case WM_CLOSE:
		if (g_bAffinityDialogChangesMade)
		{
			if (LOWORD(MessageBox(hDlg, L"Save changes?", PRODUCT_NAME, MB_ICONQUESTION | MB_YESNO)) == IDYES)
			{
				// transfer to IDOK handler
				PostMessage(hDlg, WM_COMMAND, IDOK, 0);
				return TRUE;
			}
		}
		// return UNSET_AFFINITY_MASK to indicate cancel
		pInOut = (unsigned __int64 *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
		if (pInOut)
		{
			*pInOut = UNSET_AFFINITY_MASK;
		}
		EndDialog(hDlg, UNSET_AFFINITY_MASK);
		return TRUE;
	case WM_COMMAND:
		// if a NUMA node was set or unset, clear CPU selection
		if (LOWORD(wParam) >= IDC_NUMA_0 && LOWORD(wParam) <= IDC_NUMA_7)
		{
			SyncNumaNodeSelectionWithCPUs(hDlg);
			break;
		}
		// if a CPU was set or unset, clear NUMA checkboxes
		if (LOWORD(wParam) >= IDC_CPU0 && LOWORD(wParam) <= IDC_CPU0 + MAX_GUI_SUPPORTED_CPUS)
		{
			ClearNumaNodeCheckboxes(hDlg);
			break;
		}
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_CLEAR:
			// clear
			for (nI = 0; nI < MAX_GUI_SUPPORTED_CPUS; nI++)
			{
				if (IsWindowEnabled(GetDlgItem(hDlg, IDC_CPU0 + nI)))
				{
					CheckDlgButton(hDlg, IDC_CPU0 + nI, BST_UNCHECKED);
				}
			}
			ClearNumaNodeCheckboxes(hDlg);
			return TRUE;
		case IDC_BUTTON_INVERT_SELECTION:
			// invert	
			for (nI = 0; nI < MAX_GUI_SUPPORTED_CPUS; nI++)
			{
				if (IsWindowEnabled(GetDlgItem(hDlg, IDC_CPU0 + nI)))
				{
					if (IsDlgButtonChecked(hDlg, IDC_CPU0 + nI) == BST_CHECKED)
					{
						CheckDlgButton(hDlg, IDC_CPU0 + nI, BST_UNCHECKED);
					}
					else
					{
						CheckDlgButton(hDlg, IDC_CPU0 + nI, BST_CHECKED);
					}
				}
			}
			ClearNumaNodeCheckboxes(hDlg);
			return TRUE;
		case IDOK:			
			dwAffinity = GetCPUAffinityFromDialog(hDlg);
			if (0 == dwAffinity)
			{
				MessageBox(hDlg, L"ERROR: You must choose at least one CPU!", PRODUCT_NAME, MB_ICONSTOP);
				return TRUE;
			}
			pInOut = (unsigned __int64 *)GetWindowLongPtr(hDlg, GWLP_USERDATA);
			if (pInOut)
			{
				*pInOut = (unsigned __int64)dwAffinity;
			}
			EndDialog(hDlg, 1);
			return TRUE;
		case IDCANCEL:			
			PostMessage(hDlg, WM_CLOSE, 0, 0);
			return TRUE;
		default:
			if ((LOWORD(wParam) >= IDC_CPU0) && (LOWORD(wParam) - IDC_CPU0) < MAX_GUI_SUPPORTED_CPUS)
			{				
				g_bAffinityDialogChangesMade = true;
				return TRUE;
			}
		}
		break;
	}
	return FALSE;
}
