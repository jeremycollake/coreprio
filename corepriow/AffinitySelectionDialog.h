/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#pragma once

// maximum size of a processor group in Windows and a ulonglong bitmask
#define MAX_GUI_SUPPORTED_CPUS 64

// arbitrary limit based only on our dialog display limitation
#define MAX_NUMA_NODES 8

// 0 is invalid as an affinity since no bits set
#define UNSET_AFFINITY_MASK 0

INT_PTR CALLBACK SelectAffinityDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void InitializeNumaNodeCheckboxes(HWND hDlg);
void ClearNumaNodeCheckboxes(HWND hDlg);
void SyncNumaNodeSelectionWithCPUs(HWND hDlg);
ULONGLONG GetCPUAffinityFromDialog(HWND hDlg);
void ClearSpecificCPUAffinityInDialog(HWND hDlg, ULONGLONG nAffinityMask);
void SetCPUAffinityInDialog(HWND hDlg, ULONGLONG nAffinityMask);