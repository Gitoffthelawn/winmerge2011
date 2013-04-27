/* ExternalArchiveFormat.cpp: Drive command line tools through Merge7z::Format

Copyright (c) 2005 Jochen Tucht

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Last change: 2013-04-27 by Jochen Neubeck
*/
#include "stdafx.h"
#include "Merge.h"
#include "7zCommon.h"
#include "paths.h"
#include "environment.h"
#include "LanguageSelect.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#include "ExternalArchiveFormat.h"
#include "RunModal.h"

/**
 * @brief Turn spaces into non-breaking spaces, and break string into lines
 */
static String SP2NBSP(LPCTSTR pszText, String::size_type nWidth = 120)
{
	String strText = pszText;
	string_replace(strText, _T("\x20"), _T("\xA0"));
	if (nWidth)
	{
		String::size_type i = (strText.length() + nWidth - 1) / nWidth * nWidth;
		while (i > nWidth)
		{
			strText.insert(i -= nWidth, 1, _T('\n'));
		}
	}
	return strText;
}

/**
 * @brief Simple wrapper around WINAPI GetCurrentDirectory()
 */
static String GetCurrentDirectory()
{
	String strDir;
	if (DWORD cchDir = ::GetCurrentDirectory(0, NULL))
	{
		strDir.resize(cchDir - 1);
		::GetCurrentDirectory(cchDir, &strDir.front());
	}
	return strDir;
}

/**
 * @brief Encapsulate good old INI file APIs (as far as we need them)
 */
class CExternalArchiveFormat::Profile
{
	String m_strFileName;
public:
	Profile(LPCTSTR);
	DWORD GetSize() const;
	DWORD GetProfileString(LPCTSTR, LPCTSTR, LPTSTR, DWORD) const;
	DWORD GetProfileSection(LPCTSTR, LPTSTR, DWORD) const;
};

/**
 * @brief Get full path to INI file in constructor so we have it at hand later
 */
CExternalArchiveFormat::Profile::Profile(LPCTSTR lpName)
	: m_strFileName(env_ExpandVariables(lpName))
{
}

/**
 * @brief Get total size of INI file
 */
DWORD CExternalArchiveFormat::Profile::GetSize() const
{
	DWORD size = 0;
	HANDLE h = CreateFile(m_strFileName.c_str(),
		FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
		0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (h != INVALID_HANDLE_VALUE)
	{
		size = GetFileSize(h, 0);
		CloseHandle(h);
	}
	return size;
}

/**
 * @brief Simple wrapper around WINAPI GetPrivateProfileString()
 */
DWORD CExternalArchiveFormat::Profile::GetProfileString(LPCTSTR lpAppName, LPCTSTR lpKeyName, LPTSTR lpReturnedString, DWORD nSize) const
{
	return GetPrivateProfileString(lpAppName, lpKeyName, NULL, lpReturnedString, nSize, m_strFileName.c_str());
}

/**
 * @brief Simple wrapper around WINAPI GetPrivateProfileSection()
 */
DWORD CExternalArchiveFormat::Profile::GetProfileSection(LPCTSTR lpAppName, LPTSTR lpReturnedString, DWORD nSize) const
{
	return GetPrivateProfileSection(lpAppName, lpReturnedString, nSize, m_strFileName.c_str());
}

/**
 * @brief CExternalArchiveFormat constructor
 */
CExternalArchiveFormat::CExternalArchiveFormat(const Profile *pProfile, LPCTSTR format)
: m_bLongPathPrefix(TRUE)
, m_nBulkSize(1)
, m_cchCmdMax(4095)
{
	TCHAR tmp[1024];
	if (pProfile->GetProfileString(format, _T("DeCompress"), tmp, _countof(tmp)))
	{
		m_strCmdDeCompress = env_ExpandVariables(tmp);
	}
	if (pProfile->GetProfileString(format, _T("Compress"), tmp, _countof(tmp)))
	{
		m_strCmdCompress = env_ExpandVariables(tmp);
	}
	if (pProfile->GetProfileString(format, _T("BulkSize"), tmp, _countof(tmp)))
	{
		if (int ival = PathParseIconLocation(tmp))
			m_cchCmdMax = ival;
		if (int ival = StrToInt(tmp))
			m_nBulkSize = ival;
	}
	if (pProfile->GetProfileString(format, _T("LongPathPrefix"), tmp, _countof(tmp)))
	{
		m_bLongPathPrefix = PathMatchSpec(tmp, _T("1;yes;true"));
	}
}

/**
 * @brief Not so simple wrapper around WINAPI GetShortPathName()
 * If first call to WINAPI GetShortPathName() fails, and dwAttributes specifies
 * either FILE_ATTRIBUTE_FILE or FILE_ATTRIBUTE_DIRECTORY, any missing items
 * on the path, including the file or directory at the end of the path, will be
 * created for the second call to WINAPI GetShortPathName() to succeed. Then, if
 * the item at the end of the path is a file, and its short name turns out to be
 * identical to its long name, the item will be deleted again. This is because
 * I've come across one archive tool that got confused about the empty file.
 * If users should experience this problem, they can work around it by using a
 * short filename.
 */
String CExternalArchiveFormat::GetShortPathName(LPCTSTR pchPath, DWORD dwAttributes)
{
	String strShortPath;
	int cchPath = lstrlen(pchPath);
	strShortPath.resize(cchPath);
	LPTSTR pchShortPath = &strShortPath.front();
	if (::GetShortPathName(pchPath, pchShortPath, cchPath + 1) == 0 && dwAttributes)
	{
		LPTSTR pchNext = PathSkipRoot(pchPath);
		while ((pchNext = StrChr(pchNext, '\\')) != NULL)
		{
			*pchNext = '\0';
			CreateDirectory(pchPath, NULL);
			*pchNext = '\\';
			++pchNext;
		}
		if (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			CreateDirectory(pchPath, NULL);
		}
		else
		{
			HANDLE h = CreateFile(pchPath,
				FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
				0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
			if (h != INVALID_HANDLE_VALUE)
				CloseHandle(h);
		}
		if
		(
			::GetShortPathName(pchPath, pchShortPath, cchPath + 1)
		&&	(dwAttributes & FILE_ATTRIBUTE_NORMAL)
		&&	lstrcmpi(PathFindFileName(pchPath), PathFindFileName(pchShortPath)) == 0
		)
		{
			DeleteFile(pchPath);
		}
	}
	return pchShortPath;
}

/**
 * @brief Replace placeholder within command line by given path
 * If placeholder is not quoted, path will be transformed to short names.
 */
int CExternalArchiveFormat::SetPath(String &strCmd, LPCTSTR placeholder, LPCTSTR path, DWORD attributes) const
{
	if (!m_bLongPathPrefix)
	{
		path = paths_UndoMagic(wcsdupa(path));
	}
	int nQuotedPlaceholder = strCmd.find(placeholder) + 1;
	int cchPreReplace = strCmd.length();
	int nReplace = string_replace(strCmd, placeholder + 1,
		nQuotedPlaceholder ? path : GetShortPathName(path, attributes & ~FILE_ATTRIBUTE_HIDDEN).c_str());
	if (nReplace == 1 && nQuotedPlaceholder && attributes & FILE_ATTRIBUTE_HIDDEN)
	{
		int cchPostReplace = strCmd.length();
		nQuotedPlaceholder += cchPostReplace + lstrlen(placeholder + 1) - cchPreReplace;
		if (int nBeyondTrailingQuote = strCmd.find(_T('"'), nQuotedPlaceholder) + 1)
		{
			String strTrailing(strCmd.c_str() + nQuotedPlaceholder, nBeyondTrailingQuote - nQuotedPlaceholder);
			strTrailing.insert(0U, placeholder);
			strTrailing.insert(0U, 1U, _T('\x20'));
			strCmd.insert(nBeyondTrailingQuote, strTrailing);
		}
	}
	return nReplace;
}

/**
 * @brief Run program in a modal fashion; Show an error popup on failure
 */
int CExternalArchiveFormat::RunModal(LPCTSTR lpCmd, LPCTSTR lpDir, UINT uStyle)
{
	int response = 0;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWMINNOACTIVE;
	DWORD dwError = ::RunModal(si, lpCmd, lpDir);
	if (FAILED(dwError))
	{
		String msg = SP2NBSP(lpCmd) + _T(":\n\n") + OException(dwError).msg;
		response = theApp.DoMessageBox(msg.c_str(), uStyle);
	}
	else if (UINT uExitCode = dwError)
	{
		String msg = SP2NBSP(lpCmd) + _T(":\n\n") +
			LanguageSelect.Format(IDS_EXITCODE, uExitCode);
		response = theApp.DoMessageBox(msg.c_str(), uStyle);
	}
	return response;
}

/**
 * @brief Merge7z::Format Extraction method
 */
HRESULT CExternalArchiveFormat::DeCompressArchive(HWND, LPCTSTR path, LPCTSTR folder)
{
	String strCmd = m_strCmdDeCompress;
	SetPath(strCmd, _T("\"<archive>"), path);
	SetPath(strCmd, _T("\"<dir>"), folder, FILE_ATTRIBUTE_DIRECTORY);
	return RunModal(strCmd.c_str(), folder, MB_ICONSTOP);
}

/**
 * @brief Merge7z::Format Compression method
 */
HRESULT CExternalArchiveFormat::CompressArchive(HWND, LPCTSTR path, Merge7z::DirItemEnumerator *etor)
{
	String strRestoreDir = GetCurrentDirectory();
	int response = 0;
	int nProcessed = 0;
	String strDir;
	String strCmd;
	String strItem;
	for (int i = etor->Open() ; response != IDCANCEL && i-- ; )
	{
		Merge7z::DirItemEnumerator::Item etorItem;
		etorItem.Mask.Item = 0;
		Merge7z::Envelope *envelope = etor->Enum(etorItem);
		if (etorItem.Mask.Item)
		{
			int cchDir = lstrlen(etorItem.FullPath) - lstrlen(etorItem.Name) - 1;
			if
			(
				cchDir >= 0
			&&	etorItem.FullPath[cchDir] == '\\'
			&&	lstrcmpi(etorItem.FullPath + cchDir + 1, etorItem.Name) == 0
			)
			{
				String strDirAhead(etorItem.FullPath, cchDir);
				if
				(
					strDir != strDirAhead
				||	nProcessed % m_nBulkSize == 0
				||	strCmd.length() + lstrlen(etorItem.Name) > m_cchCmdMax
				)
				{
					if (nProcessed)
					{
						SetPath(strCmd, _T("\"<filename>"), strItem.c_str());
						response = RunModal(strCmd.c_str(), strDir.c_str(), MB_ICONSTOP | MB_OKCANCEL);
					}
					strDir = strDirAhead;
					SetCurrentDirectory(strDir.c_str()); // Need this to shorten relative paths
					strCmd = m_strCmdCompress;
					SetPath(strCmd, _T("\"<archive>"), path, FILE_ATTRIBUTE_NORMAL);
					SetPath(strCmd, _T("\"<dir>"), strDir.c_str());
				}
				else
				{
					SetPath(strCmd, _T("\"<filename>"), strItem.c_str(), FILE_ATTRIBUTE_HIDDEN);
				}
				strItem = etorItem.Name;
				++nProcessed;
			}
			else
			{
				response = LanguageSelect.FormatMessage(IDS_CANT_STORE_1_AS_2,
					SP2NBSP(etorItem.FullPath), SP2NBSP(etorItem.Name)
				).MsgBox(MB_ICONSTOP | MB_OKCANCEL);
			}
		}
		if (envelope)
		{
			envelope->Free();
		}
	}
	if (nProcessed)
	{
		SetPath(strCmd, _T("\"<filename>"), strItem.c_str());
		response = RunModal(strCmd.c_str(), strDir.c_str(), MB_ICONSTOP | MB_OKCANCEL);
	}
	SetCurrentDirectory(strRestoreDir.c_str());
	return response;
}

/**
 * @brief Get the Profile * to access ExternalArchiveFormat.ini
 */
CExternalArchiveFormat::Profile const *CExternalArchiveFormat::GetProfile()
{
	static Profile prof = _T("%SupplementFolder%\\ExternalArchiveFormat.ini");
	return &prof;
}

/**
 * @brief Return a Merge7z::Format * to handle given archive file
 */
Merge7z::Format *CExternalArchiveFormat::GuessFormat(LPCTSTR path)
{
	const Profile *const pProfile = GetProfile();
	const LPCTSTR ext = PathFindExtension(path);
	TCHAR format[MAX_PATH];
	Merge7z::Format *pFormat = NULL;
	if (pProfile->GetProfileString(_T("extensions"), ext, format, _countof(format)))
	{
		// Remove end-of-line comments (in string returned from GetPrivateProfileString)
		// that is, remove semicolon & whatever follows it
		if (LPTSTR p = StrChr(format, ';'))
		{
			*p = '\0';
			StrTrim(format, _T(" \t"));
		}
		if (*format == _T('.'))
		{
			// Have Merge7z.dll process the file as per the given extension
			path = format;
		}
		else
		{
			// Bypass the Merge7z.dll
			path = NULL;
			if (lstrcmpi(format, _T("none")))
			{
				// Rather invoke an external command line tool
				typedef stl::map<String, stl::auto_ptr<CExternalArchiveFormat> > Map;
				static Map rgFormats;
				Map::iterator lookup = rgFormats.find(format);
				if (lookup == rgFormats.end())
				{
					// This overload of map::insert() goes beyond the standard.
					// It avoids dependency on a value type copy constructor.
					lookup = rgFormats.insert(format).first;
					lookup->second.reset(new CExternalArchiveFormat(pProfile, format));
				}
				pFormat = lookup->second.get();
			}
		}
	}
	if (path != NULL)
	{
		ASSERT(pFormat == NULL);
		pFormat = Merge7z->GuessFormat(path);
	}
	return pFormat;
}

/**
 * @brief Build file filter string to be passed to CFileDialog constructor
 */
/*String CExternalArchiveFormat::GetOpenFileFilterString()
{
	String strFileFilter;
	Profile const *pProfile = GetProfile();
	if (DWORD cchProfile = pProfile->GetSize())
	{
		strFileFilter.resize(cchProfile);
		LPTSTR pchProfile = &strFileFilter.front();
		cchProfile = pProfile->GetProfileSection(_T("extensions"), pchProfile, cchProfile);
		strFileFilter.resize(cchProfile + 1);
		if (cchProfile)
		{
			int i = 0;
			LPCTSTR pszKey;
			while (int cchKey = lstrlen(pszKey = strFileFilter.c_str() + i))
			{
				LPTSTR pszValue = StrChr(pszKey, _T('='));
				String strInsert;
				if (pszValue && lstrcmpi(pszValue + 1, _T("none")))
				{
					cchKey = lstrlen(pszValue + 1);
					*pszValue = _T('\0');
					strInsert.append_sprintf(_T("%s (%s)|*"), pszKey + 1, pszValue + 1);
					*pszValue = _T('|');
					strFileFilter.insert(i, strInsert);
					i += strInsert.length() + static_cast<int>(pszValue - pszKey) + 1;
				}
				strFileFilter.erase(i, cchKey + 1);
			}
			strFileFilter.resize(i);
		}
	}
	return strFileFilter;
}*/
