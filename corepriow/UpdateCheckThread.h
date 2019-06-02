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

unsigned long TextVersionToULONG(const WCHAR* pwszVer);

bool DownloadText(const WCHAR *pwszProductName,
	const WCHAR *pwszUrl,
	char** ppszRetBuffer,
	unsigned int * pnRetBufSize = NULL);

bool DownloadBinaryFile(const WCHAR* pwszProductName,
	const WCHAR* pwszUrl,
	unsigned char** ppszRetBuffer,
	unsigned int* pnRetBufSize = NULL);

bool PerformAutomaticUpdate();