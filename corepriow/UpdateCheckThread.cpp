/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*
* WARNING: Legacy code. Not factored for general reuse. Cleaned up slightly, but don't reuse.
*
*/
#include "stdafx.h"
#include "UpdateCheckThread.h"
#include "../libcoreprio/productoptions.h"

// constants shared by internet code
namespace BitsumInet
{
	const int DownloadBlockSize = 1024 * 16;
	const int MaxSavePath = 1024 * 4;
};

unsigned long TextVersionToULONG(const WCHAR* pwszTextVersion)
{	
	_ASSERT(pwszTextVersion);
	if (!pwszTextVersion || wcslen(pwszTextVersion)<7) return 0;
	unsigned int n1, n2, n3, n4 = 0;
	swscanf_s(pwszTextVersion, L"%u.%u.%u.%u", &n1, &n2, &n3, &n4);
	return (n1 << 24 | n2 << 16 | n3 << 8 | n4);
}

// referencer must use event passed to UpdateCheckThread to signal when/if filled (todo: should remove in refactor)
CString g_csLatestVersionText=L"0.0.0.0";

//
// UpdateCheckThread
//
// lpV == HEVENT to signal completion
// returns 0 if successfully completed (or checks disabled)
//
DWORD WINAPI UpdateCheckThread(LPVOID lpV)
{	
	_ASSERT(lpV);
	HANDLE hUpdateCheckCompletedEvent = (HANDLE)lpV;

	// reset to indicate g_csLatestVersionText is in invalid state
	ResetEvent(hUpdateCheckCompletedEvent);

	ProductOptions prodOptions_user(HKEY_CURRENT_USER, PRODUCT_NAME);
	bool bUpdateChecksDisabled = prodOptions_user[UPDATE_CHECK_REG_VALUE_NAME];
	if (bUpdateChecksDisabled)
	{
		// notify check completed if disabled
		if (hUpdateCheckCompletedEvent)
		{
			SetEvent(hUpdateCheckCompletedEvent);
		}
		return 0;
	}
	
	char *pszVersionBuffer = NULL;
	if (false == DownloadText(PRODUCT_NAME, UPDATE_CHECK_URL, &pszVersionBuffer) || NULL == pszVersionBuffer || 0 == strlen(pszVersionBuffer))
	{
		if (pszVersionBuffer != NULL)
		{
			delete pszVersionBuffer;
		}
		DEBUG_PRINT(L"ERROR checking update");
		return 1;
	}
		
	g_csLatestVersionText = CA2W(pszVersionBuffer);
	
	if (hUpdateCheckCompletedEvent)
	{
		SetEvent(hUpdateCheckCompletedEvent);
	}	
	delete pszVersionBuffer;	
	return 0;
}

//
// DownloadText
//
bool DownloadText(const WCHAR *pwszProductName,
	const WCHAR *pwszUrl,
	char **ppszRetBuffer,
	unsigned int *pnRetBufSize)
{
	// use download binary file, but slap a null terminator on it
	unsigned char* ppszStageBuf=NULL;
	unsigned int nStageResSize = 0;
	if (false == DownloadBinaryFile(pwszProductName, pwszUrl, static_cast<unsigned char**>(&ppszStageBuf), &nStageResSize)
		|| 0==nStageResSize
		|| NULL==*ppszStageBuf)
	{
		ppszRetBuffer = NULL;
		if (pnRetBufSize) *pnRetBufSize = 0;
		return false;
	}
	*ppszRetBuffer = new char[nStageResSize+1];
	memcpy(*ppszRetBuffer, ppszStageBuf, nStageResSize);
	(*ppszRetBuffer)[nStageResSize] = 0;
	if (pnRetBufSize) *pnRetBufSize = nStageResSize;
	delete ppszStageBuf;
	return true;
}

//
// DownloadBinaryFile
//
bool DownloadBinaryFile(const WCHAR *pwszProductName,
	const WCHAR *pwszUrl,
	unsigned char **ppszNewBuffer,
	unsigned int *pnReturnedBufferSizeInBytes)
{
	HINTERNET hInternetFile;
	HINTERNET hConnection;

	DEBUG_PRINT(L"DownloadFile URL: %s\n", pwszUrl);
	
	if (!(hConnection = InternetOpenW(pwszProductName, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0)))
	{
		DEBUG_PRINT(L"\n ERROR: Open Internet 1");
		return false;
	}

	if (!(hInternetFile = InternetOpenUrlW(hConnection, pwszUrl, NULL, 0, INTERNET_FLAG_DONT_CACHE, 0)))
	{
		DEBUG_PRINT(L"ERROR - AS_ERROR_CONNECT_GENERAL");
		InternetCloseHandle(hConnection);
		return false;
	}

	int nCurrentBufferSize = BitsumInet::DownloadBlockSize;
	*ppszNewBuffer = new unsigned char[BitsumInet::DownloadBlockSize];
	int nTotalBytesRead = 0;
	DWORD dwBytesRead;

	while (InternetReadFile(hInternetFile, (*ppszNewBuffer) + nTotalBytesRead,	BitsumInet::DownloadBlockSize,	&dwBytesRead)
		&& dwBytesRead)
	{
		nTotalBytesRead += dwBytesRead;

		// reallocate the buffer, expanding by block size		
		unsigned char *pExpanded = new unsigned char[nTotalBytesRead + BitsumInet::DownloadBlockSize];
		memcpy(pExpanded, *ppszNewBuffer, nTotalBytesRead);
		delete *ppszNewBuffer;
		*ppszNewBuffer = pExpanded;
	}

	InternetCloseHandle(hConnection);

	if (pnReturnedBufferSizeInBytes)
	{
		DEBUG_PRINT(L"Returned download size of %d bytes", nTotalBytesRead);
		*pnReturnedBufferSizeInBytes = nTotalBytesRead;
	}

	return true;
}

//
// PerformAutomaticUpdate
//
//  1. Downloads appropriate installer
//  2. Launches installer with /S switch
//  3. Caller terminates
//
bool PerformAutomaticUpdate()
{
	CString csTargetSavePath;
	WCHAR wszTemp[BitsumInet::MaxSavePath] = { 0 };

	// first set target download path
	// beware of CryptoPrevent blocks on some common temp paths (sigh)
	GetTempPath(_countof(wszTemp), wszTemp);
	// cryptoprevent forces us to use a subdir - make sure it exists first
	csTargetSavePath.Format(L"%s\\%s", wszTemp, PRODUCT_NAME);
	CreateDirectory(csTargetSavePath, NULL);
	csTargetSavePath.Format(L"%s\\%s\\%s", wszTemp, PRODUCT_NAME, INSTALLER_FILENAME);		// our subfolder to adhere to CryptoPrevent compliance

	DEBUG_PRINT(L"Download save path: %s", csTargetSavePath);

	unsigned char *pBuffer = NULL;		// dynamically allocated and retunred by DownloadFile if success
	unsigned int nDownloadSizeInBytes = 0;
	if (false == DownloadBinaryFile(PRODUCT_NAME, INSTALLER_DOWNLOAD_URL, &pBuffer, &nDownloadSizeInBytes) || NULL == pBuffer || 0==nDownloadSizeInBytes)
	{
		DEBUG_PRINT(L"ERROR downloading file");
		return false;
	}
	HANDLE hFile = CreateFile(csTargetSavePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		DEBUG_PRINT(L"ERROR creating file");
		return false;
	}
	DWORD dwBytesWritten = 0;
	if (FALSE == WriteFile(hFile, pBuffer, nDownloadSizeInBytes, &dwBytesWritten, NULL) || dwBytesWritten != nDownloadSizeInBytes)
	{
		DEBUG_PRINT(L"ERROR writing file");
		CloseHandle(hFile);
		return false;
	}
	CloseHandle(hFile);

	DEBUG_PRINT(L"Launching installer ...");

	// create command line arguments with arg0 quote encapsulated and /S switch for silent install
	CString csArgs;
	csArgs.Format(L"\"%s\" /S", csTargetSavePath.GetBuffer());

	// do NOT quote encapsulate param 1 of CreateProcess
	STARTUPINFO sInfo;
	PROCESS_INFORMATION pInfo;
	memset(&sInfo, 0, sizeof(sInfo));
	memset(&pInfo, 0, sizeof(pInfo));

	DEBUG_PRINT(L"Launching %s -- command line: %s", csTargetSavePath, csArgs);

	// set reg value before we try to execute in case we get terminated by installer
	ProductOptions prodOptions_user(HKEY_CURRENT_USER, PRODUCT_NAME);
	prodOptions_user.set_value(COREPRIO_UPDATE_FINISHED_REGVAL, true);

	BOOL bR = CreateProcess(csTargetSavePath, csArgs.GetBuffer(), NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &pInfo); 	

	if (FALSE == bR || NULL == pInfo.hProcess)
	{
		DEBUG_PRINT(L"ERROR Launching %s", csTargetSavePath);
		prodOptions_user.set_value(COREPRIO_UPDATE_FINISHED_REGVAL, false);
	}
	if (pInfo.hProcess)
	{
		CloseHandle(pInfo.hProcess);
	}
	// caller is now expected to exit
	return bR ? true : false;
}