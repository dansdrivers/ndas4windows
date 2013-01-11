#pragma once
#include <windows.h>
#include <tchar.h>
#include <setupapi.h>
#include <xtl/xtltrace.h>
#include <msi.h>
#include <strsafe.h>
#include <atlsimpcoll.h>

typedef struct _SDF_DATA {
	DWORD RequiredMsiVersion;
	BOOL  UseMUI;
	BOOL  ConfirmLanguage;
	BOOL  UseLog;
	DWORD LogMode;
	DWORD LogAttribute;
	TCHAR MsiRedist[MAX_PATH];
	TCHAR LogFile[MAX_PATH];
	TCHAR Transforms[MAX_PATH];
} SDF_DATA, *PSDF_DATA;

class SetupDefinition : public _SDF_DATA
{
public:

	SetupDefinition();
	~SetupDefinition();

	BOOL Load(LPCTSTR InfFilePath);
	void Close();

	BOOL SetActiveLanguage(LANGID LangID);
	LANGID GetActiveLanguage() const;
	void GetLanguages(ATL::CSimpleArray<LANGID>& LangIdArray);

	LPCTSTR GetMsiDatabase() const;
	LPCTSTR GetMsiCommandLine() const;

private:

	HINF m_hInf;

	mutable CString m_database;
	mutable CString m_cmdline;
	int m_activeLangIndex;
	CSimpleArray<LANGID> m_langIds;
	CSimpleArray<CString> m_langSections;
};

