#include "stdafx.h"
#include "sdf.h"

namespace {

template <typename T>
inline 
T
GetInfValueEx(HINF hInf, LPCTSTR Section, LPCTSTR Key, DWORD FieldIndex, T Default)
{
	INFCONTEXT infContext;
	if (SetupFindFirstLine(hInf, Section, Key, &infContext))
	{
		int value;
		if (SetupGetIntField(&infContext, FieldIndex, &value))
		{
			return static_cast<T>(value);
		}
	}
	return Default;
}

template <typename T>
inline 
T
GetInfValue(HINF hInf, LPCTSTR Section, LPCTSTR Key, T Default)
{
	return GetInfValueEx(hInf, Section, Key, 1, Default);
}

// Returns the string allocated from the process heap
inline
void
GetInfStringEx(
	HINF hInf, 
	LPCTSTR Section, 
	LPCTSTR Key, 
	DWORD FieldIndex,
	LPTSTR Buffer, 
	DWORD BufferSize,
	LPCTSTR Default)
{
	INFCONTEXT infContext;
	if (!SetupFindFirstLine(hInf, Section, Key, &infContext))
	{
		StringCchCopy(Buffer, BufferSize, Default);
		return;
	}
	DWORD requiredSize;
	if (!SetupGetStringField(&infContext, FieldIndex, Buffer, BufferSize, &requiredSize))
	{
		StringCchCopy(Buffer, BufferSize, Default);
		return;
	}
}

inline
void
GetInfLineText(
	HINF hInf, 
	LPCTSTR Section, 
	LPCTSTR Key, 
	LPTSTR Buffer, 
	DWORD BufferSize,
	LPCTSTR Default)
{
	INFCONTEXT infContext;
	if (!SetupFindFirstLine(hInf, Section, Key, &infContext))
	{
		StringCchCopy(Buffer, BufferSize, Default);
		return;
	}
	DWORD requiredSize;
	if (!SetupGetLineText(&infContext, NULL, NULL, NULL, Buffer, BufferSize, &requiredSize))
	{
		StringCchCopy(Buffer, BufferSize, Default);
		return;
	}
}

inline
void
GetInfString(
	HINF hInf, 
	LPCTSTR Section, 
	LPCTSTR Key, 
	LPTSTR Buffer, 
	DWORD BufferSize,
	LPCTSTR Default)
{
	return GetInfStringEx(hInf, Section, Key, 1, Buffer, BufferSize, Default);
}

void 
ParseLogMode(
	LPCTSTR LogModeString, 
	DWORD& LogMode, 
	DWORD& LogAttribute)
{
	const struct {
		TCHAR Option;
		DWORD LogMode;
		DWORD LogAttribute;
	} OptionDefinitions[] = {
		'i', INSTALLLOGMODE_INFO, 0,
		'w', INSTALLLOGMODE_WARNING, 0,
		'e', INSTALLLOGMODE_ERROR, 0,
		'a', INSTALLLOGMODE_ACTIONSTART, 0,
		'r', INSTALLLOGMODE_ACTIONDATA, 0,
		'u', INSTALLLOGMODE_USER, 0,
		'c', INSTALLLOGMODE_COMMONDATA, 0,
		'm', INSTALLLOGMODE_FATALEXIT, 0,
		'o', INSTALLLOGMODE_OUTOFDISKSPACE, 0,
		'v', INSTALLLOGMODE_VERBOSE, 0,
		'x', INSTALLLOGMODE_EXTRADEBUG, 0,
		'+', 0, INSTALLLOGATTRIBUTES_APPEND,
		'!', 0, INSTALLLOGATTRIBUTES_FLUSHEACHLINE,
		'*', INSTALLLOGMODE_INFO | INSTALLLOGMODE_WARNING | 
		INSTALLLOGMODE_ERROR | INSTALLLOGMODE_ACTIONSTART | 
		INSTALLLOGMODE_ACTIONDATA | INSTALLLOGMODE_USER | 
		INSTALLLOGMODE_COMMONDATA | INSTALLLOGMODE_FATALEXIT |
		INSTALLLOGMODE_OUTOFDISKSPACE, 0 /*,
		'#', INSTALLLOGMODE_LOGONLYONERROR, 0 */
	};

	LogMode = 0;
	LogAttribute = 0;
	for (LPCTSTR p = LogModeString; *p != 0; ++p)
	{
		for (int i = 0; i < RTL_NUMBER_OF(OptionDefinitions); ++i)
		{
			if (OptionDefinitions[i].Option == *p)
			{
				LogMode |= OptionDefinitions[i].LogMode;
				LogAttribute |= OptionDefinitions[i].LogAttribute;
				break;
			}
		}
	}
}

} // namespace

SetupDefinition::SetupDefinition()
{
	ZeroMemory(this, sizeof(_SDF_DATA));
	m_hInf = INVALID_HANDLE_VALUE;
	m_activeLangIndex = -1;
}

SetupDefinition::~SetupDefinition()
{
	if (m_hInf != INVALID_HANDLE_VALUE)
	{
		SetupCloseInfFile(m_hInf);
	}
}

void
SetupDefinition::Close()
{
	if (m_hInf != INVALID_HANDLE_VALUE)
	{
		SetupCloseInfFile(m_hInf);
	}
	m_hInf = INVALID_HANDLE_VALUE;
}

BOOL
SetupDefinition::SetActiveLanguage(LANGID LangID)
{
	int size = m_langIds.GetSize();
	for (int i = 0; i < size; ++i)
	{
		if (LangID == m_langIds[i])
		{
			m_activeLangIndex = i;
			return TRUE;
		}
	}

	return FALSE;
}

LANGID
SetupDefinition::GetActiveLanguage() const
{
	if (-1 == m_activeLangIndex)
	{
		return 0;
	}
	return m_langIds[m_activeLangIndex];
}

void 
SetupDefinition::GetLanguages(ATL::CSimpleArray<LANGID>& LangIdArray)
{
	LangIdArray = m_langIds;
}

LPCTSTR 
SetupDefinition::GetMsiDatabase() const
{
	if (-1 != m_activeLangIndex)
	{
		TCHAR database[MAX_PATH];
		GetInfString(m_hInf, 
			m_langSections[m_activeLangIndex], 
			_T("Database"), database, MAX_PATH, 
			_T(""));
		if (database[0] != 0)
		{
			m_database = CString(database);
		}
	}
	return m_database;
}

void
GetProperties(CString& cmdline, HINF hInf, LPCTSTR Section)
{
	cmdline.Empty();
	INFCONTEXT infContext;
	if (SetupFindFirstLine(hInf, Section, NULL, &infContext))
	{
		TCHAR key[MAX_PATH];
		TCHAR line[MAX_PATH];
		DWORD requiredSize;
		do
		{
			if (!SetupGetStringField(&infContext, 0, 
				key, MAX_PATH, &requiredSize))
			{
				continue;
			}
			if (!SetupGetLineText(&infContext, NULL, NULL, NULL, 
				line, MAX_PATH, &requiredSize))
			{
				continue;
			}
			cmdline += CString(key);
			cmdline += _T('=');
			cmdline += CString(line);
			cmdline += _T(' ');
		} while (SetupFindNextLine(&infContext, &infContext));
	}
}

LPCTSTR 
SetupDefinition::GetMsiCommandLine() const
{
	CString sectionName = 
		(-1 != m_activeLangIndex) ?
		m_langSections[m_activeLangIndex] :
		_T("Default");
	sectionName += _T(".Properties");
	GetProperties(m_cmdline, m_hInf, sectionName);
	return m_cmdline;
}

BOOL 
SetupDefinition::Load(
	LPCTSTR SdfFilePath)
{
	UINT ErrorLine;
	m_hInf = SetupOpenInfFile(
		SdfFilePath, 
		NULL,
		INF_STYLE_WIN4 | INF_STYLE_CACHE_DISABLE, 
		&ErrorLine);
	
	if (INVALID_HANDLE_VALUE == m_hInf)
	{
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"SetupOpenInfFile failed at line %d, error=0x%X\n", ErrorLine, GetLastError());
		return FALSE;
	}

	// MSI definitions
	RequiredMsiVersion = GetInfValue(m_hInf, _T("MSI"), _T("RequiredVersion"), 200);
	MsiRedist[0] = 0;
	DWORD required;
	// GetInfLineText(m_hInf, _T("MSI"), _T("RedistW"), MsiRedist, MAX_PATH, _T("instmsiw.exe"));
	GetInfString(m_hInf, _T("MSI"), _T("RedistW"), MsiRedist, MAX_PATH, _T("instmsiw.exe"));
	TCHAR cmdopt[MAX_PATH];
	GetInfString(m_hInf, _T("MSI"), _T("RedistC"), cmdopt, MAX_PATH, _T(""));
	if (cmdopt[0] != _T('\0'))
	{
		ATLVERIFY(SUCCEEDED(StringCchCat(MsiRedist, MAX_PATH, _T(" /c:\""))));
		ATLVERIFY(SUCCEEDED(StringCchCat(MsiRedist, MAX_PATH, cmdopt)));
		ATLVERIFY(SUCCEEDED(StringCchCat(MsiRedist, MAX_PATH, _T("\""))));
	}

	// MUI definitions
	UseMUI = GetInfValue(m_hInf, _T("MUI"), _T("UseMUI"), FALSE);
	ConfirmLanguage = GetInfValue(m_hInf, _T("MUI"), _T("Confirm"), FALSE);

	// default
	GetInfString(
		m_hInf, _T("Default"), _T("Database"), 
		m_database.GetBuffer(MAX_PATH), MAX_PATH, 
		_T("setup.msi"));
	m_database.ReleaseBuffer();

	TCHAR Buffer[MAX_PATH]; Buffer[0] = 0;
	GetInfString(m_hInf, _T("Default"), _T("LogFile"), Buffer, MAX_PATH, _T(""));

	if (Buffer[0])
	{
		ExpandEnvironmentStrings(Buffer, LogFile, MAX_PATH);
		TCHAR LogModeString[30];
		GetInfStringEx(m_hInf, _T("MSI"), _T("LogFile"), 2, LogModeString, RTL_NUMBER_OF(LogModeString), _T("v#"));
		ParseLogMode(LogModeString, LogMode, LogAttribute);
		UseLog = TRUE;
	}
	else
	{
		UseLog = FALSE;
	}

	// Languages
	INFCONTEXT infContext;
	if (SetupFindFirstLine(m_hInf, _T("Languages"), NULL, &infContext))
	{
		do
		{
			int langValue;
			if (!SetupGetIntField(&infContext, 0, &langValue))
			{
				// on failure ignore the language
				continue;
			}
			TCHAR sectionName[32];
			DWORD requiredSize;
			if (!SetupGetStringField(&infContext, 1, sectionName, 32, &requiredSize))
			{
				// on failure ignore the language
				continue;
			}
			m_langIds.Add(static_cast<LANGID>(langValue));
			m_langSections.Add(CString(sectionName));
		} while (SetupFindNextLine(&infContext, &infContext));
	}

	// If there are less than 2 languages do not use MUI
	if (m_langIds.GetSize() < 2)
	{
		UseMUI = FALSE;
		SetActiveLanguage(m_langIds[0]);
	}

	return TRUE;
}
