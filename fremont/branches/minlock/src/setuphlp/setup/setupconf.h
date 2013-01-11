#pragma once
#include "msi.h"

class CSetupLauncher
{
public:

	CSetupLauncher();

	int Run(int argc, TCHAR** argv);

private:

	CString m_configFile;

	BOOL GetSupportingLanguages(CSimpleArray<LANGID>& languages);
	BOOL GetPromptMUI();
	UINT GetRequiredMsiEngineVersion();
	BOOL GetMsiEngineInstaller(CString& command);
	void AppendTransforms(LPCTSTR section, CString& str);
	void AppendProperties(LPCTSTR section, CString& str);

	INSTALLUILEVEL m_installUILevel;
	DWORD m_logMode;
	DWORD m_logAttr;
	CString m_logFile;
	LANGID m_lgid;

	void ParseInstallUILevel(int argc, TCHAR** argv);
	void ParseLogMode(int argc, TCHAR** argv);
	LANGID GetLanguageIDFromCommandLine(int argc, TCHAR** argv);

	void GetMsiDatabase(CString& database);
	void GetMsiCommandLine(CString& cmdLine);
};


// msiexec /i ndas.msi TRANSFORMS=1033.mst
// msiexec /i ndas.msi TRANSFORMS=1033.mst;CUSTOM.mst
// msiexec /i ndas.msi REINSTALL=ALL REINSTALLMODE=vmous TRANSFORMS=1042.mst
