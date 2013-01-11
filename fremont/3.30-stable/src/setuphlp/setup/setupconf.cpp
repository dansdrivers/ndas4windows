#include "stdafx.h"
#include "setupconf.h"
#include "argv.h"
#include "resource.h"
#include "winutil.h"

#include "langdlg.h"
#include "preparedlg.h"
#include "errordlg.h"

#define DLOAD_USE_SEH
#include "dload_msi.h"
#include <xtl/xtlautores.h>

#include "dload_kernel32.h"

namespace
{
	inline LANGID ChineseCharSetLangID(LANGID lgid)
	{
		// Differentiate two Chinese character sets
		// Only returns SUBLANG_CHINESE_TRADITIONAL or SIMPLIFIED
		// Traditional Chinese:
		//  SUBLANG_CHINESE_TRADITIONAL;
		//  SUBLANG_CHINESE_HONGKONG;
		// Simplified Chinese:
		//  SUBLANG_CHINESE_SIMPLIFIED;
		//  SUBLANG_CHINESE_MACAU;
		//  SUBLANG_CHINESE_SINGAPORE;
		switch (SUBLANGID(lgid))
		{
		case SUBLANG_CHINESE_TRADITIONAL:
		case SUBLANG_CHINESE_HONGKONG:
			return SUBLANG_CHINESE_TRADITIONAL;
		case SUBLANG_CHINESE_SIMPLIFIED:
		case SUBLANG_CHINESE_SINGAPORE:
		case SUBLANG_CHINESE_MACAU:
			return SUBLANG_CHINESE_SIMPLIFIED;
		}
		return 0;
	}
	inline bool IsSameChineseCharset(LANGID lgid1, LANGID lgid2)
	{
		return ChineseCharSetLangID(lgid1) == ChineseCharSetLangID(lgid2);
	}
	inline bool IsCompatibleLanguageID(LANGID lgid1, LANGID lgid2)
	{
		if (PRIMARYLANGID(lgid1) != PRIMARYLANGID(lgid2))
		{
			return false;
		}
		if (PRIMARYLANGID(lgid1) == LANG_CHINESE)
		{
			return IsSameChineseCharset(lgid1,lgid2);
		}
		return true;
	}
	LANGID MatchLanguageID(LANGID langID, CSimpleArray<LANGID>& supports)
	{
		LANGID candidate = 0;
		int len = supports.GetSize();
		for (int i = 0; i < len; ++i)
		{
			// exact match
			if (langID == supports[i])
			{
				return langID;
			}
			// set candidate if they are compatible if there is no candidate yet.
			if (0 == candidate && IsCompatibleLanguageID(langID, supports[i]))
			{
				candidate = supports[i];
			}
		}
		if (0 == candidate)
		{
			// fallback is the first supported language id
			ATLASSERT(len > 0);
			candidate = (len > 0) ? supports[0] : 0;
		}
		return candidate;
	}

	CString GetSetupINIFileName(CString &strPadding)
	{
		CString fileName;
		LPTSTR buffer = fileName.GetBuffer(MAX_PATH);
		DWORD count = ::GetModuleFileName(NULL, buffer, MAX_PATH);
		ATLASSERT(count > 0);
		SHLWAPI::PathRenameExtension(buffer, (LPCTSTR)(strPadding + _T(".ini")));
		fileName.ReleaseBuffer();
		return fileName;
	}

	CString ExpandFilePath(LPCTSTR filePath)
	{
		TCHAR buffer[MAX_PATH];
		DWORD n = ExpandEnvironmentStrings(filePath, buffer, RTL_NUMBER_OF(buffer));
		ATLASSERT(n > 0);
		CString newFilePath;
		for (int i = 0; i < 9999; ++i)
		{
			newFilePath = buffer;
			CString exp; exp.Format(_T("%04d"), i);
			newFilePath.Replace(_T("*"), exp);
			if (!PathFileExists(newFilePath))
			{
				return newFilePath;
			}
		}
		return newFilePath;
	}

	CString ExpandFilePath(LPCTSTR path, LPCTSTR fileName)
	{
		TCHAR fullPath[MAX_PATH] = {0};
		lstrcpyn(fullPath, path, RTL_NUMBER_OF(fullPath));
		PathAppend(fullPath, fileName);
		return ExpandFilePath(fullPath);
	}

	LANGID
	GetUserDefaultUILanguageEx()
	{
		// GetUserDefaultUILanguage is not available in Windows 98
		// It is only available in Windows Me, 2000, XP or later
		typedef LANGID (WINAPI* PFN)(void);
		PFN pfn;
		HMODULE hModule = LoadLibrary(_T("kernel32.dll"));
		pfn = (PFN)GetProcAddress(hModule, "GetUserDefaultUILanguage");
		if (NULL == pfn)
		{
			pfn = (PFN)GetProcAddress(hModule, "GetUserDefaultLangID");
			ATLASSERT(pfn);
		}
		LANGID langid = pfn();
		FreeLibrary(hModule);
		return langid;
	}
}

CSetupLauncher::CSetupLauncher()
{
	
}

BOOL
CSetupLauncher::GetSupportingLanguages(
	CSimpleArray<LANGID>& SupportingLanguages)
{
	LPCTSTR SectionName = _T("MUI");
	TCHAR keyNames[256] = {0};

	// read all keys in [MUI] section
	DWORD charCount = WINBASE::GetPrivateProfileString(
		SectionName, NULL, NULL, 
		keyNames, RTL_NUMBER_OF(keyNames), 
		m_configFile);

	// iterate all key names
	for (LPCTSTR keyName = &keyNames[0];
		_T('\0') != *keyName && keyName < &keyNames[charCount];
		keyName += ::lstrlen(keyName) + 1)
	{
		if (0 == ::lstrcmpi(keyName, _T("PROMPT")))
		{
			// ignore this
			continue;
		}
		// otherwise language code in decimal notation
		else
		{
			int langid = 0;
			BOOL ret = SHLWAPI::StrToIntEx(keyName, STIF_DEFAULT, &langid);
			// if the conversion fails, we ignore that key
			if (!ret)
			{
				ATLTRACE("Invalid key name: %ws\n", keyName);
				continue;
			}
			// 1033=0 or 1
			UINT enabled = ::GetPrivateProfileInt(SectionName, keyName, 0, m_configFile);
			if (enabled)
			{
				// is valid locale id in the system?
				if (WINNLS::IsValidLocale(MAKELCID(langid,SORT_DEFAULT), LCID_INSTALLED))
				{
					SupportingLanguages.Add(static_cast<LANGID>(langid));
				}
			}
		}
	}
	return TRUE;
}

BOOL
CSetupLauncher::GetPromptMUI()
{
	return ::GetPrivateProfileInt(_T("MUI"), _T("PROMPT"), 0, m_configFile);
}

UINT
CSetupLauncher::GetRequiredMsiEngineVersion()
{
	return ::GetPrivateProfileInt(_T("MSIEngine"), _T("Version"), 200, m_configFile);
}

BOOL
CSetupLauncher::GetMsiEngineInstaller(CString& command)
{
	DWORD len = ::GetPrivateProfileString(
		_T("MSIEngine"), _T("Installer"), _T(""), 
		command.GetBuffer(MAX_PATH), MAX_PATH,
		m_configFile);
	command.ReleaseBuffer(len);
	return (len > 0);
}

BOOL ParseLogModeOption(LPCTSTR opt, DWORD& mode, DWORD& attr)
{
	mode = 0;
	attr = 0;
	for (LPCTSTR p = opt; *p != _T('\0'); ++p)
	{
		switch (*p) 
		{
		case 'a': mode |= INSTALLLOGMODE_ACTIONSTART; break;
		case 'c': mode |= INSTALLLOGMODE_COMMONDATA; break;
		case 'e': mode |= INSTALLLOGMODE_ERROR; break;
		case 'i': mode |= INSTALLLOGMODE_INFO; break;
		case 'm': mode |= INSTALLLOGMODE_FATALEXIT; break;
		case 'o': mode |= INSTALLLOGMODE_OUTOFDISKSPACE; break;
		case 'p': mode |= INSTALLLOGMODE_PROPERTYDUMP; break;
		case 'r': mode |= INSTALLLOGMODE_ACTIONDATA; break;
		case 'u': mode |= INSTALLLOGMODE_USER; break;
		case 'v': mode |= INSTALLLOGMODE_VERBOSE; break;
		case 'w': mode |= INSTALLLOGMODE_WARNING; break;
		case '*': mode |= INSTALLLOGMODE_ACTIONSTART | INSTALLLOGMODE_COMMONDATA |
		     INSTALLLOGMODE_ERROR | INSTALLLOGMODE_INFO | 
			 INSTALLLOGMODE_FATALEXIT | INSTALLLOGMODE_OUTOFDISKSPACE |
			 INSTALLLOGMODE_PROPERTYDUMP | INSTALLLOGMODE_ACTIONDATA |
			 INSTALLLOGMODE_USER | INSTALLLOGMODE_WARNING;
			break;
		case '+': attr |= INSTALLLOGATTRIBUTES_APPEND; break;
		case '!': attr |= INSTALLLOGATTRIBUTES_FLUSHEACHLINE; break;
		default:
			mode = 0;
			attr = 0;
			return FALSE;
		}
	}
	return TRUE;
}

LANGID
CSetupLauncher::GetLanguageIDFromCommandLine(int argc, TCHAR** argv)
{
	// from the command line options
	for (int i = 0; i < argc; ++i)
	{
		if ((argv[i][0] == _T('-') || argv[i][0] == _T('/')) &&
			0 == lstrcmpi(_T("lang"), &argv[i][1]))
		{
			// log file should be specified
			if (i + 1 >= argc)
			{
				return 0;
			}
			int value = 0;
			if (!StrToIntEx(argv[i+1], STIF_SUPPORT_HEX, &value))
			{
				return 0;
			}
			return static_cast<LANGID>(value);
		}
	}
	return 0;
}

void
CSetupLauncher::ParseLogMode(int argc, TCHAR** argv)
{
	// from the command line options
	for (int i = 0; i < argc; ++i)
	{
		if ((argv[i][0] == _T('-') || argv[i][0] == _T('/')) &&
			argv[i][1] == _T('l'))
		{
			// log file should be specified
			if (i + 1 >= argc)
			{
				goto ini;
			}
			// log options
			if (!ParseLogModeOption(&argv[i][2], m_logMode, m_logAttr))
			{
				goto ini;
			}
			// expand log file name
			m_logFile = ExpandFilePath(argv[i+1]);
		}
	}

	// from setup.ini
ini:

	TCHAR logpath[MAX_PATH] = {0};
	GetPrivateProfileString(
		_T("Logging"), _T("Path"), _T("%TEMP%"), 
		logpath, RTL_NUMBER_OF(logpath), m_configFile);

	TCHAR logtype[32] = {0};
	GetPrivateProfileString(
		_T("Logging"), _T("Type"), _T(""),
		logtype, RTL_NUMBER_OF(logtype), m_configFile);

	TCHAR logtpl[MAX_PATH] = {0};
	GetPrivateProfileString(
		_T("Logging"), _T("Template"), _T(""),
		logtpl, RTL_NUMBER_OF(logtpl), m_configFile);

	if (logtype[0] != _T('\0') && logtpl[0] != _T('\0'))
	{
		if (!ParseLogModeOption(logtype, m_logMode, m_logAttr))
		{
			return;
		}
		m_logFile = ExpandFilePath(logpath, logtpl);
		ATLTRACE("Log file: %ls\n", m_logFile);
	}
}

void
CSetupLauncher::ParseInstallUILevel(int argc, TCHAR** argv)
{
	// from the command line options
	for (int i = 0; i < argc; ++i)
	{
		if ((argv[i][0] == _T('-') || argv[i][0] == _T('/')) &&
			argv[i][1] == _T('q'))
		{
			struct { LPCTSTR option; INSTALLUILEVEL level; } opts[] = {
				_T(""),   INSTALLUILEVEL_BASIC,
				_T("b"),  INSTALLUILEVEL_BASIC,
				_T("b-"), (INSTALLUILEVEL)(INSTALLUILEVEL_BASIC | INSTALLUILEVEL_PROGRESSONLY),
				_T("b+"), (INSTALLUILEVEL)(INSTALLUILEVEL_BASIC | INSTALLUILEVEL_ENDDIALOG),
				_T("f"),  INSTALLUILEVEL_FULL,
				_T("n"),  INSTALLUILEVEL_NONE,
				_T("n+"), (INSTALLUILEVEL)(INSTALLUILEVEL_NONE | INSTALLUILEVEL_ENDDIALOG),
				_T("r"),  INSTALLUILEVEL_REDUCED
			};
			// if the command line option is set, setup.ini is not consulted at all
			for (int j = 0; j < RTL_NUMBER_OF(opts); ++j)
			{
				if (0 == lstrcmpi(&argv[i][2], opts[j].option))
				{
					m_installUILevel = opts[j].level;
					return;
				}
			}
			// invalid command line option? -- ignored
			ATLTRACE("Invalid logging option: %ls", argv[i]);
		}
	}

	// setup.ini in [Display] section
	struct { LPCTSTR display; INSTALLUILEVEL level; } map[] = {
		_T("full"), INSTALLUILEVEL_FULL,
		_T("none"), INSTALLUILEVEL_NONE,
		_T("quiet"), (INSTALLUILEVEL)(INSTALLUILEVEL_BASIC | INSTALLUILEVEL_PROGRESSONLY),
		_T("basic"), INSTALLUILEVEL_BASIC,
		_T("reduced"), INSTALLUILEVEL_REDUCED
	};

	TCHAR display[12] = {0};
	::GetPrivateProfileString(
		_T("Display"), _T("Display"), _T("full"), 
		display, RTL_NUMBER_OF(display), 
		m_configFile);

	TCHAR completion[8] = {0};
	::GetPrivateProfileString(
		_T("Display"), _T("CompletionNotice"), _T("yes"),
		completion, RTL_NUMBER_OF(completion), 
		m_configFile);

	m_installUILevel = (0 == lstrcmpi(completion, _T("no"))) ?
		(INSTALLUILEVEL)0 : INSTALLUILEVEL_ENDDIALOG;

	for (int i = 0; i < RTL_NUMBER_OF(map); ++i)
	{
		if (0 == ::lstrcmpi(display, map[i].display))
		{
			m_installUILevel = (INSTALLUILEVEL)(m_installUILevel | map[i].level);
			return;
		}
	}

	// default fall-back
	m_installUILevel = (INSTALLUILEVEL)(m_installUILevel | INSTALLUILEVEL_FULL);
	return;
}

void 
CSetupLauncher::GetMsiDatabase(CString& database)
{
	GetPrivateProfileString(
		_T("MSI"), _T("MSI"), 
		_T(""), database.GetBuffer(MAX_PATH), MAX_PATH,
		m_configFile);
	database.ReleaseBuffer();
}

void
CSetupLauncher::AppendTransforms(LPCTSTR section, CString& transforms)
{
	TCHAR keyNames[256] = {0};

	// read all keys in the section
	DWORD charCount = WINBASE::GetPrivateProfileString(
		section, NULL, NULL, 
		keyNames, RTL_NUMBER_OF(keyNames), 
		m_configFile);

	// iterate all key names
	for (LPCTSTR keyName = &keyNames[0];
		_T('\0') != *keyName && keyName < &keyNames[charCount];
		keyName += lstrlen(keyName) + 1)
	{
		CString transform;
		GetPrivateProfileString(
			section, keyName, _T(""), 
			transform.GetBuffer(MAX_PATH), MAX_PATH, 
			m_configFile);
		transform.ReleaseBuffer();
		if (!transform.IsEmpty())
		{
			if (!transforms.IsEmpty())
			{
				transforms += ";";
			}
			transforms += transform;
		}
	}
}

void 
CSetupLauncher::AppendProperties(LPCTSTR section, CString& cmdLine)
{
	TCHAR keyNames[256] = {0};

	// read all keys in the section
	DWORD charCount = WINBASE::GetPrivateProfileString(
		section, NULL, NULL, 
		keyNames, RTL_NUMBER_OF(keyNames), 
		m_configFile);

	// iterate all key names
	for (LPCTSTR keyName = &keyNames[0];
		_T('\0') != *keyName && keyName < &keyNames[charCount];
		keyName += lstrlen(keyName) + 1)
	{
		CString value;
		GetPrivateProfileString(
			section, keyName, _T(""), 
			value.GetBuffer(MAX_PATH), MAX_PATH, 
			m_configFile);
		value.ReleaseBuffer();
		if (value.IsEmpty() || -1 != value.Find(' '))
		{
			value = _T("\"") + value + _T("\"");
		}
		cmdLine += _T(' ');
		cmdLine += keyName;
		cmdLine += _T('=');
		cmdLine += value;
	}
}

void
CSetupLauncher::GetMsiCommandLine(CString& cmdLine)
{
	CString transforms;
	
	// MST
	CString section = _T("MST");
	AppendTransforms(section, transforms); 

	// MST.1033
	section.Format(_T("MST.%d"), m_lgid);
	AppendTransforms(section, transforms);

	ATLTRACE("Transforms=%ls\n", transforms);

	// Add transforms to the cmdline
	if (!transforms.IsEmpty())
	{
		cmdLine += _T(" TRANSFORMS=") + transforms;
	}

	// Options
	section = _T("Options");
	AppendProperties(section, cmdLine);

	// Options.1033
	section.Format(_T("Options.%d"), m_lgid);
	AppendProperties(section, cmdLine);

	ATLTRACE("CmdLine=%ls\n", cmdLine);
}

int 
CSetupLauncher::Run(int argc, TCHAR** argv)
{
	// set the current directory as the directory where setup.exe is.
	TCHAR moduleDirectory[MAX_PATH] = {0};

	DWORD len = GetModuleFileName(NULL, moduleDirectory, MAX_PATH);
	ATLASSERT(len > 0);

	BOOL success = PathRemoveFileSpec(moduleDirectory);
	ATLASSERT(success);

	success = SetCurrentDirectory(moduleDirectory);
	ATLASSERT(success);


	// Full path of setup.ini
	XTL::AutoModuleHandle hModule = LoadLibrary(Kernel32Dll::GetModuleName());
	Kernel32Dll kernel32(hModule);

	// find architecture specific setup file (ex: setup.i386.ini)
	m_configFile.Empty();
	if (kernel32.IsProcAvailable("GetNativeSystemInfo"))
	{
		SYSTEM_INFO SystemInfo = {0,};
		kernel32.GetNativeSystemInfo(&SystemInfo);

		switch (SystemInfo.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_INTEL:
			m_configFile = GetSetupINIFileName(CString(_T(".i386")));
			break;
		case PROCESSOR_ARCHITECTURE_IA64:
			m_configFile = GetSetupINIFileName(CString(_T(".ia64")));
			break;
		case PROCESSOR_ARCHITECTURE_AMD64:
			m_configFile = GetSetupINIFileName(CString(_T(".amd64")));
			break;
		case PROCESSOR_ARCHITECTURE_UNKNOWN:
		default:
			m_configFile = GetSetupINIFileName(CString(_T(".unknown")));
			break;
		}

		if (!SHLWAPI::PathFileExists(m_configFile))
		{
			m_configFile.Empty();
		}
	}

	if (m_configFile.IsEmpty())
	{
		m_configFile = GetSetupINIFileName(CString(_T("")));
	}

	// does setup.ini exist?
	if (!SHLWAPI::PathFileExists(m_configFile))
	{
		CString msg; 
		msg.Format(IDS_ERR_LOAD_CONFIG, m_configFile);
		AtlMessageBox(NULL, static_cast<LPCTSTR>(msg), IDR_MAINFRAME, MB_OK | MB_ICONERROR);
		return ERROR_FILE_NOT_FOUND;
	}

	ParseLogMode(argc, argv);
	ParseInstallUILevel(argc, argv);

	CSimpleArray<LANGID> supportingLanguages;
	GetSupportingLanguages(supportingLanguages);

	// get the default language id from the command line
	// if the command line specifies the language, then language selection dialog
	// will not be shown
	bool fShowLangSelDlg = false;
	m_lgid = GetLanguageIDFromCommandLine(argc, argv);
	
	if (0 == m_lgid)
	{
		// if the command line is not specified, use the DefaultUILanguage
		m_lgid = GetUserDefaultUILanguageEx();

		// show language selection dialog
		// if there are multiple languages available and MUIPrompt is 1
		fShowLangSelDlg = supportingLanguages.GetSize() > 1 && GetPromptMUI();
	}

	// adjust lgid
	m_lgid = MatchLanguageID(m_lgid, supportingLanguages);

	if (fShowLangSelDlg)
	{
		CLanguageSelectionDlg langDlg;
		langDlg.SetLanguages(supportingLanguages);
		langDlg.SetSelectedLangID(m_lgid);
		INT_PTR nRet = langDlg.DoModal();
		ATLTRACE("LangSel returned %d\n", nRet);
		if (IDOK != nRet)
		{
			return ERROR_INSTALL_USEREXIT;
		}
		// change lgid
		m_lgid = langDlg.GetSelectedLangID();
	}

	UINT msiRequired = GetRequiredMsiEngineVersion();
	if (IsMsiUpgradeNecessary(msiRequired))
	{
		// MSI engine upgrade requires Admin Privilege
		if (!IsAdmin())
		{
			AtlMessageBox(NULL, IDS_REQUIRES_ADMIN_PRIV, IDR_MAINFRAME, MB_OK | MB_ICONERROR);
			return ERROR_INSTALL_FAILURE;
		}

		CString commandLine; GetMsiEngineInstaller(commandLine);
		
		TCHAR buffer[MAX_PATH] = {0};
		lstrcpyn(buffer, commandLine, MAX_PATH);

		CPrepareDlg dlg;
		dlg.DoModal(
			GetDesktopWindow(), 
			reinterpret_cast<LPARAM>(buffer));

		ATLTRACE("instmsiw.exe returns %d\n", dlg.GetExitCode());
	}

	// Install!
	XTL::AutoModuleHandle hMsiDll = LoadLibrary(_T("msi.dll"));
	if (hMsiDll.IsInvalid())
	{
		ATLTRACE(_T("Unable to load msi.dll\n"));
		return ERROR_MOD_NOT_FOUND;
	}

	MsiDll msidll(hMsiDll);

	__try
	{
		INSTALLUILEVEL uilevel = msidll.MsiSetInternalUI(m_installUILevel, NULL);
		ATLTRACE("MsiSetInternalUI returned %d (UILEVEL)\n", uilevel);

		if (m_logMode)
		{
			UINT ret = msidll.MsiEnableLog(
				m_logMode, 
				m_logFile, 
				m_logAttr);
			// Ignore MsiEnableLog error
			ATLTRACE("MsiEnableLog returned %d\n", ret);
		}

		CString msiDatabase; GetMsiDatabase(msiDatabase);
		CString msiCmdLine;  GetMsiCommandLine(msiCmdLine);

		ATLTRACE("msiDatabase=%ls\n", msiDatabase);
		ATLTRACE("msiCmdLine=%ls\n", msiCmdLine);

		UINT ret = msidll.MsiInstallProduct(msiDatabase, msiCmdLine);
		ATLTRACE("MsiInstallProduct returned %d\n", ret);

		if (ERROR_SUCCESS != ret &&
			ERROR_SUCCESS_REBOOT_INITIATED != ret &&
			ERROR_SUCCESS_REBOOT_REQUIRED != ret &&
			ERROR_INSTALL_USEREXIT != ret &&
			ERROR_INSTALL_FAILURE != ret)
		{
			LPTSTR lpszError = GetErrorMessage(ret, _Module.m_wResLangId);
			if (NULL != lpszError)
			{
				CErrorDlg::DialogParam param = { ret, lpszError };
				CErrorDlg errorDlg;
				errorDlg.DoModal(
					GetDesktopWindow(), 
					reinterpret_cast<LPARAM>(&param));
				(void) LocalFree(lpszError);
			}
		}

		return ret;
	}
	__except (
		GetExceptionCode() == DLOAD_EXCEPTION_PROC_NOT_FOUND ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ATLTRACE("DLOAD Exception: %08X\n", GetExceptionCode());
		return ERROR_INSTALL_FAILURE;
	}
}

