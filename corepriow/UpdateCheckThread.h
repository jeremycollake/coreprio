/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#pragma once

#ifndef BETA
#define UPDATE_CHECK_URL L"https://update.bitsum.com/versioninfo/coreprio/"
#else
#define UPDATE_CHECK_URL L"https://update.bitsum.com/versioninfo/coreprio/beta/"
#endif

DWORD WINAPI UpdateCheckThread(LPVOID lpV);
bool IsOnlineVersionNewer(const WCHAR *pwszCurrentVersion, const WCHAR *pwszNewVersion);
bool DownloadFile(const WCHAR *pwszProductName,
	const WCHAR *pwszUrl,
	char **ppszNewBuffer,
	int *pnReturnedBufferSizeInBytes = NULL);
bool PerformAutomaticUpdate();