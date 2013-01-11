#include "stdafx.h"
#include "downloadbsc.h"
#include "resource.h"
#include "setupui.h"

//{B506A5D1-9716-4F35-8ED5-9ECB0E9A55F8}
const GUID IID_IDownloadBindStatusCallback = 
{0xB506A5D1L,0x9716,0x4F35,{0x8E,0xD5,0x9E,0xCB,0x0E,0x9A,0x55,0xF8}};

//{00000000-9716-4F35-8ED5-9ECB0E9A55F8}
const GUID IID_IUnknown = 
{0x00000000L,0x9716,0x4F35,{0x8E,0xD5,0x9E,0xCB,0x0E,0x9A,0x55,0xF8}};

//////////////////////////////////////////////////////////////////////////
//
// CDownloadBindStatusCallback
//
//////////////////////////////////////////////////////////////////////////

CDownloadBindStatusCallback::CDownloadBindStatusCallback(
	ISetupUI* pSetupUI) : 
	m_pSetupUI(pSetupUI), 
	m_iRefCnt(1), 
	m_ulProgressSoFar(0)
{
}

CDownloadBindStatusCallback::~CDownloadBindStatusCallback()
{
}

//
// IUnknown Interface
//

HRESULT 
CDownloadBindStatusCallback::QueryInterface(const IID& riid, VOID** ppvObj)
{
	if (!ppvObj)
		return E_INVALIDARG;

	if (riid == IID_IUnknown || riid == IID_IDownloadBindStatusCallback)
	{
		*ppvObj = this;
		AddRef();
		return NOERROR;
	}
	*ppvObj = 0;
	return E_NOINTERFACE;
}

ULONG
CDownloadBindStatusCallback::AddRef()
{
	return ++m_iRefCnt;
}

ULONG
CDownloadBindStatusCallback::Release()
{
	if (--m_iRefCnt != 0)
		return m_iRefCnt;
	delete this;
	return 0;
}

//
// CDownloadBindStatusCallback::IBindStatusCallback
//

HRESULT 
CDownloadBindStatusCallback::OnProgress(
	ULONG ulProgress, 
	ULONG ulProgressMax, 
	ULONG ulStatusCode, 
	LPCWSTR szStatusText)
{
	switch (ulStatusCode)
	{
	case BINDSTATUS_CONNECTING:
		{
			m_pSetupUI->SetActionText(IDS_BSC_BEGINDOWNLOADING);
			// check for cancel
			if (m_pSetupUI->HasUserCanceled())
				return E_ABORT;
			break;
		}

	case BINDSTATUS_BEGINDOWNLOADDATA:
		{
			// initialize progress bar with max # of ticks
			m_pSetupUI->InitProgressBar(ulProgressMax);
			m_pSetupUI->SetActionText(IDS_BSC_BEGINDOWNLOADING);
			// init progress so far
			m_ulProgressSoFar = 0;

			// check for cancel
			if (m_pSetupUI->HasUserCanceled())
				return E_ABORT;

			// fall through
		}
	case BINDSTATUS_DOWNLOADINGDATA:
		{
			m_pSetupUI->SetProgressBar(ulProgress);
			//// calculate how far we have moved since the last time
			//ULONG ulProgIncrement = ulProgress - m_ulProgressSoFar;

			//// set progress so far to current value
			//m_ulProgressSoFar = ulProgress;

			//// send progress message (if we have progressed)
			//if (ulProgIncrement > 0)

			// check for cancel
			if(m_pSetupUI->HasUserCanceled())
				return E_ABORT;

			break;
		}
	case BINDSTATUS_ENDDOWNLOADDATA:
		{
			// send any remaining progress to complete download portion of progress bar
			//ULONG ulProgIncrement = ulProgressMax - m_ulProgressSoFar;
			//if (ulProgIncrement > 0)
			//	m_pSetupUI->IncrementProgressBar(ulProgIncrement);

			m_pSetupUI->SetProgressBar(ulProgress);
			m_pSetupUI->SetActionText(IDS_BSC_ENDDOWNLOADDATA);

			// check for cancel
			if(m_pSetupUI->HasUserCanceled())
				return E_ABORT;

			break;
		}
	}

	return S_OK;
}
