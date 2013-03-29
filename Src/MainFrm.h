/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MainFrm.h
 *
 * @brief Declaration file for CMainFrame
 *
 */
// ID line follows -- this is updated by SVN
// $Id$

#include "OptionsMgr.h"
#include "Constants.h"
#include "VSSHelper.h"
#include "MergeCmdLineInfo.h"
#include "../BuildTmp/Merge/midl/WinMerge_h.h"

enum { WM_NONINTERACTIVE = 888 }; // timer value

class CDiffView;
class CDirView;
class CDirFrame;
class CDocFrame;
class CHexMergeFrame;
class CMergeEditView;
class CMergeDiffDetailView;
class LineFiltersList;
struct FileLocation;

class PackingInfo;
class CLanguageSelect;

/**
 * @brief Frame class containing save-routines etc
 */
class CMainFrame
	: public OWindow
	, public IStatusDisplay
	, public CScriptable<IMergeApp>
{
	friend CLanguageSelect;
public:
	CMainFrame(HWindow *, const MergeCmdLineInfo &);

	// IElementBehaviorFactory
	STDMETHOD(FindBehavior)(BSTR, BSTR, IElementBehaviorSite *, IElementBehavior **);
	// IMergeApp
	STDMETHOD(put_StatusText)(BSTR bsStatusText);
	STDMETHOD(get_StatusText)(BSTR *pbsStatusText);
	STDMETHOD(get_Strings)(IDispatch **ppDispatch);
	STDMETHOD(ShowHTMLDialog)(BSTR url, VARIANT *arguments, BSTR features, VARIANT *ret);

// Attributes
public:	
	HWindow *m_pWndMDIClient;
	HMENU m_hMenuDefault;
	HACCEL m_hAccelTable;
	LogFont m_lfDiff; /**< MergeView user-selected font */
	LogFont m_lfDir; /**< DirView user-selected font */
// Operations
public:
	void InitCmdUI();
	template<UINT CmdID>
	void UpdateCmdUI(BYTE);
	void UpdateSourceTypeUI(UINT sourceType) { m_sourceType = sourceType; }
	void InitialActivate(int nCmdShow);
	void EnableModeless(BOOL bEnable);
	void SetBitmaps(HMENU);
	int SyncFileToVCS(LPCTSTR pszDest, BOOL &bApplyToAll, String *psError);
	bool DoFileOpen(
		FileLocation &filelocLeft,
		FileLocation &filelocRight,
		DWORD dwLeftFlags = FFILEOPEN_DETECT,
		DWORD dwRightFlags = FFILEOPEN_DETECT,
		int nRecursive = 0,
		CDirFrame * = NULL);
	bool DoFileOpen(
		PackingInfo &packingInfo,
		UINT idCompareAs,
		FileLocation &filelocLeft,
		FileLocation &filelocRight,
		DWORD dwLeftFlags,
		DWORD dwRightFlags,
		int nRecursive,
		CDirFrame * = NULL);
	void ShowMergeDoc(CDirFrame *,
		FileLocation & filelocLeft,
		FileLocation & filelocRight,
		DWORD dwLeftFlags = 0,
		DWORD dwRightFlags = 0,
		PackingInfo * infoUnpacker = NULL);
	void ShowHexMergeDoc(CDirFrame *,
		const FileLocation &,
		const FileLocation &,
		BOOL bLeftRO, BOOL bRightRO);
	void UpdateResources();
	static bool CreateBackup(BOOL bFolder, LPCTSTR pszPath);
	int HandleReadonlySave(String &strSavePath, BOOL bMultiFile, BOOL &bApplyToAll);
	void SetStatus(UINT);
	void SetStatus(LPCTSTR);
	void UpdateIndicators();
	void ApplyViewWhitespace();
	void SetEOLMixed(bool bAllow);
	void SelectFilter();
	void ShowVSSError(HRESULT, LPCTSTR strItem);
	void ShowHelp(LPCTSTR helpLocation = NULL);
	void UpdateCodepageModule();
	void CheckinToClearCase(LPCTSTR strDestinationPath);
	void StartFlashing();
	bool AskCloseConfirmation();
	void DoOpenMrgman(LPCTSTR);
	bool DoOpenConflict(LPCTSTR);
	bool LoadAndOpenProjectFile(LPCTSTR);
	HWindow *CreateChildHandle() const;
	CDocFrame *GetActiveDocFrame(BOOL *pfActive = NULL);
	void SetActiveMenu(HMENU);

	HMenu *SetScriptMenu(HMenu *, LPCSTR section);

	static void OpenFileToExternalEditor(LPCTSTR file, LPCTSTR editor = NULL);
	void OpenFileWith(LPCTSTR file) const;
	bool ParseArgsAndDoOpen(const MergeCmdLineInfo &);

	void RecalcLayout();
	BOOL PreTranslateMessage(MSG *);

// Implementation methods
protected:
	virtual ~CMainFrame();
// Implementation in SourceControl.cpp
	void InitializeSourceControlMembers();
	BOOL SaveToVersionControl(LPCTSTR pszSavePath);
// End SourceControl.cpp


// Public implementation data
public:
	String m_strSaveAsPath; /**< "3rd path" where output saved if given */
	VSSHelper m_vssHelper; /**< Helper class for VSS integration */
	bool m_bFlashing; /**< Window is flashing. */
	bool m_bEscShutdown; /**< If commandline switch -e given ESC closes appliction */
	bool m_bClearCaseTool; /**< WinMerge is executed as an external Rational ClearCase compare/merge tool. */
	MergeCmdLineInfo::ExitNoDiff m_bExitIfNoDiff; /**< Exit if files are identical? */

	/**
	 * @name Version Control System (VCS) integration.
	 */
	/*@{*/ 
protected:
	CMyComBSTR m_strVssUser; /**< Visual Source Safe User ID */
	CMyComBSTR m_strVssPassword; /**< Visual Source Safe Password */
	String m_strVssDatabase; /**< Visual Source Safe database */
	String m_strCCComment; /**< ClearCase comment */
public:
	BOOL m_bCheckinVCS;     /**< TRUE if files should be checked in after checkout */
	BOOL m_CheckOutMulti; /**< Suppresses VSS int. code asking checkout for every file */
	BOOL m_bVssSuppressPathCheck; /**< Suppresses VSS int code asking about different path */
	/*@}*/

	/** @brief Possible toolbar image sizes. */
	enum TOOLBAR_SIZE
	{
		TOOLBAR_SIZE_16x16,
		TOOLBAR_SIZE_32x32,
	};

	HTabCtrl *GetTabBar() { return m_wndTabBar; }
	HStatusBar *GetStatusBar() { return m_wndStatusBar; }
// Implementation data
protected:
	// control bar embedded members
	HStatusBar *m_wndStatusBar;
	HToolBar *m_wndToolBar;
	HTabCtrl *m_wndTabBar;

	HImageList *m_imlMenu;
	HImageList *m_imlToolbarEnabled; /**< Images for toolbar */
	HImageList *m_imlToolbarDisabled; /**< Images for toolbar */

	/**
	 * A structure attaching a menu item, icon and menu types to apply to.
	 */
	struct MENUITEM_ICON
	{
		WORD menuitemID;   /**< Menu item's ID. */
		WORD iconResID;    /**< Icon's resource ID. */
	};

	static const MENUITEM_ICON m_MenuIcons[];

// Generated message map functions
protected:
	void GetMrgViewFontProperties();
	void GetDirViewFontProperties();
	template<UINT>
	LRESULT OnWndMsg(WPARAM, LPARAM);
	LRESULT WindowProc(UINT, WPARAM, LPARAM);
	void OnOptionsShowDifferent();
	void OnOptionsShowIdentical();
	void OnOptionsShowUniqueLeft();
	void OnOptionsShowUniqueRight();
	void OnOptionsShowBinaries();
	void OnOptionsShowSkipped();
	void OnFileOpen();
	void OnFileClose();
	void OnHelpGnulicense();
	void OnOptions();
	void OnViewSelectfont();
	void OnViewUsedefaultfont();
	void OnHelpContents();
	void OnViewWhitespace();
	void OnToolsGeneratePatch();
	void OnSaveConfigData();
	void OnFileNew();
	void OnToolsFilters();
	void OnDebugLoadConfig();
	void OnViewStatusBar();
	void OnViewToolbar();
	void OnViewTabBar();
	void OnResizePanes();
	void OnFileOpenProject();
	void OnWindowCloseAll();
	void OnSaveProject();
	void OnDebugResetOptions();
	void OnToolbarNone();
	void OnToolbarSmall();
	void OnToolbarBig();
	void OnToolTipText(NMHDR *);
	void OnHelpReleasenotes();
	void OnHelpTranslations();
	void OnFileOpenConflict();

private:
	void addToMru(LPCTSTR szItem, LPCTSTR szRegSubKey, UINT nMaxItems = 20);
	BOOL IsComparing();
	void RedisplayAllDirDocs();
	void SaveWindowPlacement();
	bool PrepareForClosing();
	CChildFrame *GetMergeDocToShow(CDirFrame *);
	CHexMergeFrame *GetHexMergeDocToShow(CDirFrame *);
	CDirFrame *GetDirDocToShow();
	void UpdateDirViewFont();
	void UpdateMrgViewFont();
	void OpenFileOrUrl(LPCTSTR szFile, LPCTSTR szUrl);
	BOOL CreateToobar();
	void LoadToolbarImages();
	HMENU NewMenu(int ID);
	CDocFrame *FindFrameOfType(FRAMETYPE);
	void LoadFilesMRU();
	void SaveFilesMRU();

	String m_TitleMRU;
	stl::vector<String> m_FilesMRU;

	HMenu *m_pScriptMenu;
	String m_TitleScripts;
	stl::vector<String> m_Scripts;

	class Strings : public CMyDispatch<IDispatch>
	{
	public:
		Strings() : CMyDispatch<IDispatch>(__uuidof(IStrings)) { }
		STDMETHOD(Invoke)(DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *);
	} m_Strings;

	struct CmdState
	{
		BYTE LeftReadOnly;
		BYTE RightReadOnly;
		BYTE Refresh;
		BYTE FileEncoding;
		BYTE TreeMode;
		BYTE ShowHiddenItems;
		BYTE ExpandAllSubdirs;
		BYTE MergeCompare;
		BYTE LeftToRight;
		BYTE RightToLeft;
		BYTE Delete;
		BYTE AllLeft;
		BYTE AllRight;
		BYTE PrevDiff;
		BYTE NextDiff;
		BYTE CurDiff;
		BYTE Save;
		BYTE SaveLeft;
		BYTE SaveRight;
		BYTE Undo;
		BYTE Redo;
		BYTE Cut;
		BYTE Copy;
		BYTE Paste;
		BYTE Replace;
		BYTE SelectLineDiff;
		BYTE EolToDos;
		BYTE EolToUnix;
		BYTE EolToMac;
		BYTE GenerateReport;
		BYTE CompareSelection;
		BYTE ToggleBookmark;
		BYTE NavigateBookmarks;
		const BYTE *Lookup(UINT id);
	} m_cmdState;
	UINT m_sourceType;
};

/////////////////////////////////////////////////////////////////////////////
