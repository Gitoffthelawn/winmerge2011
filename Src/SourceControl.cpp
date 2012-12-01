/** 
 * @file  SourceControl.cpp
 *
 * @brief Implementation file for some source control-related functions.
 */
// ID line follows -- this is updated by SVN
// $Id$

#include "StdAfx.h"
#include <initguid.h>
#include "ssauto.h"
#include "Merge.h"
#include "MainFrm.h"
#include "SettingStore.h"
#include "LanguageSelect.h"
#include "OptionsDef.h"
#include "coretools.h"
#include "paths.h"
#include "VssPrompt.h"
#include "CCPrompt.h"
#include <MyCom.h>

void CMainFrame::InitializeSourceControlMembers()
{
	m_vssHelper.SetProjectBase(SettingStore.GetProfileString(_T("Settings"), _T("VssProject")));
	m_strVssUser = SettingStore.GetProfileString(_T("Settings"), _T("VssUser")).c_str();
	m_strVssDatabase = SettingStore.GetProfileString(_T("Settings"), _T("VssDatabase")).c_str();

	String vssPath = COptionsMgr::Get(OPT_VSS_PATH);
	if (vssPath.empty())
	{
		CRegKeyEx reg;
		if (reg.QueryRegMachine(_T("SOFTWARE\\Microsoft\\SourceSafe")) == ERROR_SUCCESS)
		{
			vssPath = reg.ReadString(_T("SCCServerPath"), _T(""));
			vssPath = paths_GetParentPath(vssPath.c_str()) + _T("\\Ss.exe");
			COptionsMgr::SaveOption(OPT_VSS_PATH, vssPath);
		}
	}
}

/**
* @brief Saves file to selected version control system
* @param strSavePath Path where to save including filename
* @return Tells if caller can continue (no errors happened)
* @sa CheckSavePath()
*/
BOOL CMainFrame::SaveToVersionControl(LPCTSTR pszSavePath)
{
	String strSavePath = pszSavePath;
	pszSavePath = paths_UndoMagic(&strSavePath.front());
	String path = paths_GetParentPath(pszSavePath);
	String name = PathFindFileName(pszSavePath);
	int userChoice = IDOK;
	int nVerSys = COptionsMgr::Get(OPT_VCS_SYSTEM);
	switch (nVerSys)
	{
	case VCS_NONE:	//no versioning system
		// Already handled in CheckSavePath()
		break;
	case VCS_VSS4:	// Visual Source Safe
		{
			// Prompt for user choice
			CVssPrompt dlg;
			dlg.m_strMessage = LanguageSelect.FormatMessage(IDS_SAVE_FMT, pszSavePath);
			dlg.m_strProject = m_vssHelper.GetProjectBase();
			dlg.m_strUser = m_strVssUser;          // BSP - Add VSS user name to dialog box
			dlg.m_strPassword = m_strVssPassword;

			// Dialog not suppressed - show it and allow user to select "checkout all"
			if (!m_CheckOutMulti)
			{
				dlg.m_bMultiCheckouts = FALSE;
				userChoice = LanguageSelect.DoModal(dlg);
				m_CheckOutMulti = dlg.m_bMultiCheckouts;
			}
			// process versioning system specific action
			if (userChoice != IDOK)
				return FALSE;
			WaitStatusCursor waitstatus(IDS_VSS_CHECKOUT_STATUS);
			m_vssHelper.SetProjectBase(dlg.m_strProject.c_str());
			SettingStore.WriteProfileString(_T("Settings"), _T("VssProject"), m_vssHelper.GetProjectBase().c_str());
			string_format args(_T("checkout \"%s/%s\""), m_vssHelper.GetProjectBase().c_str(), name.c_str());
			String vssPath = COptionsMgr::Get(OPT_VSS_PATH);
			if (DWORD code = RunIt(vssPath.c_str(), args.c_str(), path.c_str()))
			{
				LanguageSelect.MsgBox(code != STILL_ACTIVE ?
					IDS_VSSERROR : IDS_VSS_RUN_ERROR, MB_ICONSTOP);
				return FALSE;
			}
		}
		break;
	case VCS_VSS5: // CVisual SourceSafe 5.0+ (COM)
		{
			// prompt for user choice
			CVssPrompt dlg;
			CRegKeyEx reg;

			dlg.m_strMessage = LanguageSelect.FormatMessage(IDS_SAVE_FMT, pszSavePath);
			dlg.m_strProject = m_vssHelper.GetProjectBase();
			dlg.m_strUser = m_strVssUser;          // BSP - Add VSS user name to dialog box
			dlg.m_strPassword = m_strVssPassword;
			dlg.m_strSelectedDatabase = m_strVssDatabase;

			// Dialog not suppressed - show it and allow user to select "checkout all"
			if (!m_CheckOutMulti)
			{
				dlg.m_bMultiCheckouts = FALSE;
				userChoice = LanguageSelect.DoModal(dlg);
				m_CheckOutMulti = dlg.m_bMultiCheckouts;
			}
			// process versioning system specific action
			if (userChoice != IDOK)
				return FALSE;
			WaitStatusCursor waitstatus(IDS_VSS_CHECKOUT_STATUS);
			m_vssHelper.SetProjectBase(dlg.m_strProject.c_str());
			m_strVssUser = dlg.m_strUser.c_str();
			m_strVssPassword = dlg.m_strPassword.c_str();
			m_strVssDatabase = dlg.m_strSelectedDatabase;

			SettingStore.WriteProfileString(_T("Settings"), _T("VssDatabase"), m_strVssDatabase.c_str());
			SettingStore.WriteProfileString(_T("Settings"), _T("VssProject"), m_vssHelper.GetProjectBase().c_str());
			SettingStore.WriteProfileString(_T("Settings"), _T("VssUser"), m_strVssUser);

			HRESULT hr;
			CMyComPtr<IVSSDatabase> vssdb;
			CMyComPtr<IVSSItems> vssis;
			CMyComPtr<IVSSItem> vssi;

			// BSP - Create the COM interface pointer to VSS
			if (FAILED(hr = vssdb.CoCreateInstance(CLSID_VSSDatabase, IID_IVSSDatabase)))
			{
				ShowVSSError(hr, _T(""));
				return FALSE;
			}
			// BSP - Open the specific VSS data file  using info from VSS dialog box
			// let vss try to find one if not specified
			if (FAILED(hr = vssdb->Open(
				CMyComBSTR(!m_strVssDatabase.empty() ?
					(m_strVssDatabase + _T("\\srcsafe.ini")).c_str() : NULL),
				m_strVssUser,
				m_strVssPassword)))
			{
				ShowVSSError(hr, _T(""));
			}

			// BSP - Combine the project entered on the dialog box with the file name...
			const UINT nBufferSize = 1024;
			TCHAR buffer[nBufferSize];
			TCHAR buffer1[nBufferSize];
			TCHAR buffer2[nBufferSize];

			_tcscpy(buffer1, pszSavePath);
			_tcscpy(buffer2, m_vssHelper.GetProjectBase().c_str());
			_tcslwr(buffer1);
			_tcslwr(buffer2);

			//make sure they both have \\ instead of /
			for (int k = 0; k < nBufferSize; k++)
			{
				if (buffer1[k] == '/')
					buffer1[k] = '\\';
			}

			m_vssHelper.SetProjectBase(buffer2);
			TCHAR * pbuf2 = &buffer2[2];//skip the $/
			TCHAR * pdest =  _tcsstr(buffer1, pbuf2);
			if (pdest)
			{
				size_t index  = pdest - buffer1 + 1;
				_tcscpy(buffer, buffer1);
				name = &buffer[index + _tcslen(pbuf2)];
				if (name[0] == ':')
				{
					_tcscpy(buffer2, name.c_str());
					_tcscpy(buffer, &buffer2[2]);
					name = buffer;
				}
			}
			String strItem = m_vssHelper.GetProjectBase() + '\\' + name;
			//  BSP - ...to get the specific source safe item to be checked out
			if (FAILED(hr = vssdb->get_VSSItem(
				CMyComBSTR(strItem.c_str()), VARIANT_FALSE, &vssi)))
			{
				ShowVSSError(hr, strItem.c_str());
				return FALSE;
			}

			if (!m_bVssSuppressPathCheck)
			{
				// BSP - Get the working directory where VSS will put the file...
				CMyComBSTR bstrLocalSpec;
				vssi->get_LocalSpec(&bstrLocalSpec);
				// BSP - ...and compare it to the directory WinMerge is using.
				if (lstrcmpi(bstrLocalSpec, pszSavePath))
				{
					// BSP - if the directories are different, let the user confirm the CheckOut
					int iRes = LanguageSelect.MsgBox(IDS_VSSFOLDER_AND_FILE_NOMATCH, 
							MB_YESNO | MB_YES_TO_ALL | MB_ICONWARNING);

					if (iRes == IDNO)
					{
						m_bVssSuppressPathCheck = FALSE;
						m_CheckOutMulti = FALSE; // Reset, we don't want 100 of the same errors
						return FALSE;   // No means user has to start from begin
					}
					else if (iRes == IDYESTOALL)
						m_bVssSuppressPathCheck = TRUE; // Don't ask again with selected files
				}
			}
			// BSP - Finally! Check out the file!
			if (FAILED(hr = vssi->Checkout(
				CMyComBSTR(_T("")),
				CMyComBSTR(pszSavePath), 0)))
			{
				ShowVSSError(hr, pszSavePath);
				return FALSE;
			}
		}
		break;
	case VCS_CLEARCASE:
		{
			// prompt for user choice
			CCCPrompt dlg;
			if (!m_CheckOutMulti)
			{
				dlg.m_bMultiCheckouts = FALSE;
				dlg.m_comments = _T("");
				dlg.m_bCheckin = FALSE;
				userChoice = LanguageSelect.DoModal(dlg);
				m_CheckOutMulti = dlg.m_bMultiCheckouts;
				m_strCCComment = dlg.m_comments;
				m_bCheckinVCS = dlg.m_bCheckin;
			}
			// process versioning system specific action
			if (userChoice != IDOK)
				return FALSE;
			WaitStatusCursor waitstatus;
			// checkout operation
			string_replace(m_strCCComment, _T("\""), _T("\\\""));
			string_format args(_T("checkout -c \"%s\" \"%s\""), m_strCCComment.c_str(), name.c_str());
			String vssPath = COptionsMgr::Get(OPT_VSS_PATH);
			if (DWORD code = RunIt(vssPath.c_str(), args.c_str(), path.c_str()))
			{
				LanguageSelect.MsgBox(code != STILL_ACTIVE ?
					IDS_VSSERROR : IDS_VSS_RUN_ERROR, MB_ICONSTOP);
				return FALSE;
			}
		}
		break;
	}
	return TRUE;
}
