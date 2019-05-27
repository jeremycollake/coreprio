#pragma once 

void InstallService(const WCHAR * pszServiceName,
	const WCHAR * pszDisplayName,
	DWORD dwStartType,
	const WCHAR * pszDependencies,
	PWSTR pszAccount,
	PWSTR pszPassword);

void UninstallService(const WCHAR * pszServiceName);
