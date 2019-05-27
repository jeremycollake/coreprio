/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#pragma once

#include <map>
#include <atlstr.h>

// registry access with local cache
// ::get_value uses in-memory maps, filled dynamically as required by read_value
// ::refresh forces reload of maps on next get of each option
// ::write_value writes directly and updates map

class ProductOptions
{
	HKEY hHive;
	CString csKeyname;

	// map of options of various data types
	std::map<CString, bool> mapBools;
	std::map<CString, unsigned> mapuint32;
	std::map<CString, unsigned long long> mapuint64;
	std::map<CString, CString> mapstr;

	// overloaded for specific data types
	bool read_value(const WCHAR* pwszValueName, bool &bResult);
	bool read_value(const WCHAR* pwszValueName, unsigned &nResult);
	bool read_value(const WCHAR* pwszValueName, unsigned long long &nResult);
	bool read_value(const WCHAR* pwszValueName, CString &csResult);
		
	bool write_value(const WCHAR* pwszValueName, const bool bVal);
	bool write_value(const WCHAR* pwszValueName, const unsigned nVal);
	bool write_value(const WCHAR* pwszValueName, const unsigned long long nVal);
	bool write_value(const WCHAR* pwszValueName, const WCHAR *val);
public:
	ProductOptions(const HKEY hHive, const WCHAR* pwszProductName);

	// force a reload of any referenced options
	void refresh();

	// returns false if doesn't exist in registry
	bool& operator[] (const WCHAR* pwszValueName);	// let boolvals (only) get referenced by subscript
	bool get_value(const WCHAR* pwszValueName, bool& bVal);
	bool get_value(const WCHAR* pwszValueName, BOOL& bVal);
	bool get_value(const WCHAR* pwszValueName, unsigned& nVal);
	bool get_value(const WCHAR* pwszValueName, DWORD& nVal);
	bool get_value(const WCHAR* pwszValueName, unsigned long long& nVal);
	bool get_value(const WCHAR* pwszValueName, CString &csVal);

	// returns false if registry write failed
	bool set_value(const WCHAR* pwszValueName, const bool bVal);
	bool set_value(const WCHAR* pwszValueName, const BOOL bVal);
	bool set_value(const WCHAR* pwszValueName, const unsigned nVal);
	bool set_value(const WCHAR* pwszValueName, const DWORD nVal);
	bool set_value(const WCHAR* pwszValueName, const unsigned long long nVal);
	bool set_value(const WCHAR* pwszValueName, const WCHAR *val);
};
