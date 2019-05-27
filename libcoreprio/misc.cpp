/*
* Part of the CorePrio project
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
* https://bitsum.com/portfolio/coreprio
* See LICENSE.TXT
*/
#include "stdafx.h"
#include "misc.h"

UINT GetDlgItemTextEx(HWND hDlg, UINT nID, CString &csResult)
{
	// size not including null terminator
	size_t nSize=static_cast<size_t>(SendMessage(GetDlgItem(hDlg, nID), WM_GETTEXTLENGTH, 0, 0));
	if (!nSize)
	{
		csResult.Empty();
		return 0;
	}
	WCHAR *pwszBuf = new WCHAR[nSize + 1];
	size_t nRetSize=static_cast<size_t>(GetDlgItemText(hDlg, nID, pwszBuf, static_cast<int>(nSize + 1)));
	_ASSERT(nRetSize <= nSize + 1);
	csResult = pwszBuf;
	delete pwszBuf;
	return static_cast<UINT>(nRetSize);
}

bool wildicmpEx(const TCHAR *wild, const TCHAR *str) 
{
	_ASSERT(str);
	int slen = (int)_tcslen(str);
	int wlen = (int)_tcslen(wild);
	if (!slen || !wlen) return false;

	// if inverse operator, return inverted wildcard
	if (wild[0] == '~' || wild[0] == '!')
	{
		return !wildicmpEx(wild + 1, str);
	}
	
	// MUST exist
	int reqLen = 0;
	for (int i = 0; i < wlen; ++i)
	{
		if (wild[i] != _T('*'))
		{
			++reqLen;
		}
	}

	if (slen < reqLen)
	{
		return false;
	}

	// length is okay; now we do the comparing
	int w = 0, s = 0;

	for (; s < slen && w < wlen; ++s, ++w) {

		// if we hit a '?' just go to the next char in both `str` and `wild`
		if (wild[w] == _T('?'))
		{
			continue;
		}

		// we hit an unlimited wildcard
		else if (wild[w] == _T('*')) {
			// if it's the last char in the string, we're done
			if ((w + 1) == wlen)
				return true;

			bool ret = true;

			// for each remaining character in `wild`
			while (++w < wlen && ret)
			{
				// for each remaining character in `str`
				int i;
				for (i = 0; i < (slen - s); ++i)
				{
					// if same as the next after wildcard
					if ((str[s + i] == wild[w])
						|| (tolower(wild[w]) == tolower(str[s + i])))
					{
						// compare from these points on
						ret = wildicmpEx(wild + w, str + s + i);
						// if successful, we're done
						if (ret)
						{
							return true;
						}
					}
				}
				// important
				if (i >= (slen - s))
				{
					return false;
				}
			}
			return ret;
		}
		else   if ((wild[w] == str[s])
			// or if not wildcard (or other special char) and we compare lowercase forms
			|| (tolower(wild[w]) == tolower(str[s])))
		{
			continue;
		}
		else
		{
			return false;
		}
	}

	return s >= slen ? true : false;
}

