#ifndef _NDAS_UPDATE_UI_H_
#define _NDAS_UPDATE_UI_H_
#pragma once
#include <windows.h>
#include <urlmon.h>
#include <ndas/ndupdate.h>

/*---------------------------------------------------------------------------
 *
 * Constants
 *
 ---------------------------------------------------------------------------*/
#define MAX_STR_CAPTION 256

/*---------------------------------------------------------------------------
 *
 * Enums
 *
 ---------------------------------------------------------------------------*/
enum irmProgress // progress dialog return messages
{
    irmNotInitialized = -1, // dialog was not initialized
    irmOK             =  0, // ok
    irmCancel         =  1, // user depressed cancel button
};

/*---------------------------------------------------------------------------
 *
 * CDownloadUI class
 *
 ---------------------------------------------------------------------------*/
class NDUPDATE_API CDownloadUI
{
public:
     CDownloadUI();
     ~CDownloadUI();

    BOOL Initialize(HINSTANCE hInst, HWND hwndParent, LPCTSTR szCaption);
    BOOL Terminate();
    HWND GetCurrentWindow();
    BOOL HasUserCanceled();
    VOID SetUserCancel();
    VOID InitProgressBar(ULONG ulProgressMax);
    VOID IncrementProgressBar(ULONG ulProgress);

    irmProgress SetBannerText(LPCTSTR szBanner);
    irmProgress SetActionText(LPCTSTR szAction);

private:
    HINSTANCE m_hInst;  // handle to instance containing resources

    HWND  m_hwndProgress;    // handle to progress dialog
    HWND  m_hwndParent;      // handle to parent window
    TCHAR  m_szCaption[MAX_STR_CAPTION]; // caption
    BOOL  m_fInitialized;    // whether dialog has been initialized
    BOOL  m_fUserCancel;     // whether user has chosen to cancel
    ULONG m_ulProgressMax;   // maximum number of ticks on progress bar
    ULONG m_ulProgressSoFar; // current progress
};

/*---------------------------------------------------------------------------
 *
 * CDownloadBindStatusCallback class
 *
 ---------------------------------------------------------------------------*/

class CDownloadBindStatusCallback : public IBindStatusCallback
{
 public: // IUnknown implemented virtual functions
     HRESULT         __stdcall QueryInterface(const IID& riid, VOID** ppvObj);
     ULONG   __stdcall AddRef();
     ULONG   __stdcall Release();
 public: // IBindStatusCallback implemented virtual functions
     CDownloadBindStatusCallback(CDownloadUI* piDownloadUI);
    ~CDownloadBindStatusCallback();

    HRESULT __stdcall OnStartBinding(DWORD, IBinding*) {return S_OK;}
    HRESULT __stdcall GetPriority(LONG*) {return S_OK;}
    HRESULT __stdcall OnLowResource(DWORD ) {return S_OK;}
    HRESULT __stdcall OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText);
    HRESULT __stdcall OnStopBinding(HRESULT, LPCWSTR ) {return S_OK;}
    HRESULT __stdcall GetBindInfo(DWORD*, BINDINFO*) {return S_OK;}
    HRESULT __stdcall OnDataAvailable(DWORD, DWORD, FORMATETC*, STGMEDIUM*) {return S_OK;}
    HRESULT __stdcall OnObjectAvailable(REFIID, IUnknown*) {return S_OK;}
 private:
    CDownloadUI* m_pDownloadUI; // pointer to actual UI
    int          m_iRefCnt;
    ULONG        m_ulProgressSoFar;
};

#endif //_NDAS_UPDATE_UI_H_
