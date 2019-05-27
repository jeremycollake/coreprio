/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*
* WARNING: legacy code. May need maintenance.
*
*/
#include "stdafx.h"
#include "UpdateCheckThread.h"
#include "../libcoreprio/productoptions.h"

CString g_csLatestVersionText;

////////////////////////////////////////////////////
// helper function to parse and compare text version numbers
bool IsOnlineVersionNewer(const WCHAR *pwszCurrentVersion, const WCHAR *pwszNewVersion)
{
	// do bounds check safety
	if (NULL == pwszCurrentVersion || wcslen(pwszCurrentVersion) > 127 || NULL == pwszNewVersion || wcslen(pwszNewVersion) > 127)
	{
		DEBUG_PRINT(L"ERROR: BOUNDS CHECK IsOnlineVersionNewer");
		return false;
	}

	// cache last result if input values are the same
	static bool bBoolLastResult = false;
	static WCHAR wszLastVersion1[128] = { 0 };
	static WCHAR wszLastVersion2[128] = { 0 };
	if (_tcslen(wszLastVersion1) && _tcslen(wszLastVersion2)
		&& 0 == _tcsicmp(wszLastVersion1, pwszCurrentVersion)
		&& 0 == _tcsicmp(wszLastVersion2, pwszNewVersion))	// if we have recorded both last values
	{
		DEBUG_PRINT(L"Cache matches. Returning last compare.");
		return bBoolLastResult;
	}

	//
	// parse out verson
	//						
	unsigned int nSignificance0 = 0; // greatest (major)
	unsigned int nSignificance1 = 0;
	unsigned int nSignificance2 = 0;
	unsigned int nSignificance3 = 0; // least						
	_stscanf_s(pwszNewVersion, L"%u.%u.%u.%u", &nSignificance0, &nSignificance1, &nSignificance2, &nSignificance3);

	DEBUG_PRINT(L"Read update version %u.%u.%u.%u", nSignificance0, nSignificance1, nSignificance2, nSignificance3);

	unsigned int nExistingSignificance0 = 0; // greatest (major)
	unsigned int nExistingSignificance1 = 0;
	unsigned int nExistingSignificance2 = 0;
	unsigned int nExistingSignificance3 = 0; // least						
	_stscanf_s(pwszCurrentVersion, L"%u.%u.%u.%u", &nExistingSignificance0, &nExistingSignificance1, &nExistingSignificance2, &nExistingSignificance3);

	DEBUG_PRINT(L"Current version %u.%u.%u.%u", nExistingSignificance0, nExistingSignificance1, nExistingSignificance2, nExistingSignificance3);

	//if(g_csLatestVersionText.CompareNoCase(PARKCONTROL_CURRENT_VERSION)!=0)
	unsigned long nBuiltVerNumber = nSignificance0 << 24;
	unsigned long nBuiltCurrentVerNumber = nExistingSignificance0 << 24;
	nBuiltVerNumber |= nSignificance1 << 16;
	nBuiltVerNumber |= nSignificance2 << 8;
	nBuiltVerNumber |= nSignificance3;
	nBuiltCurrentVerNumber |= nExistingSignificance1 << 16;
	nBuiltCurrentVerNumber |= nExistingSignificance2 << 8;
	nBuiltCurrentVerNumber |= nExistingSignificance3;

	DEBUG_PRINT(L"Comparing %x with %x", nBuiltCurrentVerNumber, nBuiltVerNumber);

	// update cache
	_tcsnccpy_s(wszLastVersion1, _countof(wszLastVersion1), pwszCurrentVersion, _TRUNCATE);
	_tcsnccpy_s(wszLastVersion2, _countof(wszLastVersion2), pwszNewVersion, _TRUNCATE);

	if (nBuiltVerNumber > nBuiltCurrentVerNumber)
	{
		DEBUG_PRINT(L"UPDATE IS AVAILABLE");
		bBoolLastResult = true;
		return true;
	}
	bBoolLastResult = false;
	return false;
}


//
// pass in EVENT to signal when update check is completed as lpV
//
DWORD WINAPI UpdateCheckThread(LPVOID lpV)
{
	Sleep(1000);
	_ASSERT(lpV);
	HANDLE hUpdateCheckCompletedEvent = (HANDLE)lpV;

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
	if (false == DownloadFile(PRODUCT_NAME, UPDATE_CHECK_URL, &pszVersionBuffer) || NULL == pszVersionBuffer || 0 == strlen(pszVersionBuffer))
	{
		if (pszVersionBuffer != NULL)
		{
			delete pszVersionBuffer;
		}
		DEBUG_PRINT(L"ERROR checking update");
		return 1;
	}

	// set global boolean to TRUE and populate license text
	CA2W cW(pszVersionBuffer);
	g_csLatestVersionText = cW;
	g_csLatestVersionText.TrimLeft();
	g_csLatestVersionText.TrimRight();

	// basic check for bad result
	if (g_csLatestVersionText.GetLength() > 8
		||
		g_csLatestVersionText.GetLength() < 3
		||
		-1 == g_csLatestVersionText.Find(L"."))
	{
		DEBUG_PRINT(L"Upd bad result");
		delete pszVersionBuffer;
		pszVersionBuffer = NULL;
		if (hUpdateCheckCompletedEvent)
		{
			SetEvent(hUpdateCheckCompletedEvent);
		}
		return 0;
	}
	
	delete pszVersionBuffer;
	pszVersionBuffer = NULL;

	if (hUpdateCheckCompletedEvent)
	{
		SetEvent(hUpdateCheckCompletedEvent);
	}
	return 0;
}

bool DownloadFile(const WCHAR *pwszProductName,
	const WCHAR *pwszUrl,
	char **ppszNewBuffer,
	int *pnReturnedBufferSizeInBytes)
{
	HINTERNET hInternetFile;
	HINTERNET hConnection;

	DEBUG_PRINT(L"DoownloadFile URL: %s\n", pwszUrl);

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

#define DOWNLOAD_BLOCK_SIZE 4096

	int nCurrentBufferSize = DOWNLOAD_BLOCK_SIZE;
	*ppszNewBuffer = new char[DOWNLOAD_BLOCK_SIZE];
	int nTotalBytesRead = 0;
	DWORD dwBytesRead;

	while (!(InternetReadFile(hInternetFile,
		(*ppszNewBuffer) + nTotalBytesRead,
		DOWNLOAD_BLOCK_SIZE,
		&dwBytesRead)
		&&
		!dwBytesRead))
	{
		nTotalBytesRead += dwBytesRead;

		//
		// reallocate the buffer, expanding DOWNLOAD_BLOCK_SIZE bytes
		//
		char *pTemp;
		pTemp = new char[nTotalBytesRead];
		memcpy(pTemp, *ppszNewBuffer, nTotalBytesRead);
		delete *ppszNewBuffer;
		*ppszNewBuffer = new char[nTotalBytesRead + DOWNLOAD_BLOCK_SIZE];
		memcpy(*ppszNewBuffer, pTemp, nTotalBytesRead);
		delete pTemp;
	}

	*((*ppszNewBuffer) + nTotalBytesRead) = 0;

	DEBUG_PRINT(L"\nDownloadFile - Returned text: %S", *ppszNewBuffer);

	InternetCloseHandle(hConnection);

	if (pnReturnedBufferSizeInBytes)
	{
		DEBUG_PRINT(L"Returned download size of %d bytes", nTotalBytesRead);
		*pnReturnedBufferSizeInBytes = nTotalBytesRead;
	}

	return true;
}

// do NOT put a null terminator on binary file (only difference)
// TODO - can consolidate with DownloadFile
bool DownloadBinaryFile(const WCHAR *pwszProductName,
	const WCHAR *pwszUrl,
	char **ppszNewBuffer,
	int *pnReturnedBufferSizeInBytes)
{
	HINTERNET hInternetFile;
	HINTERNET hConnection;

	DEBUG_PRINT(L"DoownloadFile URL: %s\n", pwszUrl);


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

	int nCurrentBufferSize = DOWNLOAD_BLOCK_SIZE;
	*ppszNewBuffer = new char[DOWNLOAD_BLOCK_SIZE];
	int nTotalBytesRead = 0;
	DWORD dwBytesRead;

	while (!(InternetReadFile(hInternetFile,
		(*ppszNewBuffer) + nTotalBytesRead,
		DOWNLOAD_BLOCK_SIZE,
		&dwBytesRead)
		&&
		!dwBytesRead))
	{
		nTotalBytesRead += dwBytesRead;

		//
		// reallocate the buffer, expanding DOWNLOAD_BLOCK_SIZE bytes
		//
		char *pTemp;
		pTemp = new char[nTotalBytesRead];
		memcpy(pTemp, *ppszNewBuffer, nTotalBytesRead);
		delete *ppszNewBuffer;
		*ppszNewBuffer = new char[nTotalBytesRead + DOWNLOAD_BLOCK_SIZE];
		memcpy(*ppszNewBuffer, pTemp, nTotalBytesRead);
		delete pTemp;
	}

	// do NOT put a null terminator on binary file
	//*((*ppszNewBuffer)+nTotalBytesRead)=0;
	InternetCloseHandle(hConnection);

	if (pnReturnedBufferSizeInBytes)
	{
		DEBUG_PRINT(L"Returned download size of %d bytes", nTotalBytesRead);
		*pnReturnedBufferSizeInBytes = nTotalBytesRead;
	}

	return true;
}

/////////////////////////////////////////////////
//
// Automatic update function
//
//  1. Downloads appropriate installer
//  2. Launches installer with /S switch
//  3. Terminates immediately
//

bool PerformAutomaticUpdate()
{
	CString csTargetSavePath;
	WCHAR wszTemp[4096] = { 0 };

	// first set target download path
	// beware of CryptoPrevent blocks on some common temp paths (sigh)
	GetTempPath(_countof(wszTemp), wszTemp);
	// cryptoprevent forces us to use a subdir - make sure it exists first
	csTargetSavePath.Format(L"%s\\%s", wszTemp, PRODUCT_NAME);
	CreateDirectory(csTargetSavePath, NULL);
	csTargetSavePath.Format(L"%s\\%s\\%s", wszTemp, PRODUCT_NAME, INSTALLER_FILENAME);		// our subfolder to adhere to CryptoPrevent compliance

	DEBUG_PRINT(L"Download save path: %s", csTargetSavePath);

	char *pBuffer = NULL;		// dynamically allocated and retunred by DownloadFile if success
	int nDownloadSizeInBytes = 0;
#define MINIMUM_VALID_INSTALLER_SIZE (16*1024)
	if (false == DownloadBinaryFile(PRODUCT_NAME, INSTALLER_DOWNLOAD_URL, &pBuffer, &nDownloadSizeInBytes) || NULL == pBuffer || nDownloadSizeInBytes <= MINIMUM_VALID_INSTALLER_SIZE)
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
	csArgs.Format(L"\"%s\" /S", csTargetSavePath);

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