/////////////////////////////////////////////////////////////////////////////
// Pix.h - contains the following:
//
// 1. CPix - Picture object helper class
// 2. CClipboardFormatDlg - format select dialog
// 3. CPixClipboard - support multiple clipboard image formats
// 4. Semi-standardized image fileheader and info structures
//
// Portions Copyright (C) 2002 SoftGee LLC. All rights reserved
// Version 1.0 -- September 13, 2002.
//
// This file is for use with the Windows Template Library.
// The code and information is provided "as-is" without
// warranty of any kind, either expressed or implied.
//
// CPix is derived and extended from the MFC CPictureHolder class
// Copyright (C) 1992-1998 Microsoft Corporation. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __PIX_H__
#define __PIX_H__

#pragma once

#include <pshpack2.h>
typedef struct PIXINFO {
	short piType;
	LPCTSTR piDescription;
	SIZE piSize;
	DWORD piAttributes;
	BYTE piHeader[256];
} FAR *LPPIXINFO;
#include <poppack.h>
#include <atlmisc.h>

template <bool t_bManaged>
class CPixT
{
public:
// Attributes
    IPicture* m_pPix;
	PIXINFO m_info;
	void* m_pBuffer;

// Implementation
public:
/////////////////////////////////////
// constructor/destructor/operator //
/////////////////////////////////////
	CPixT(IPicture* pPix = NULL) : m_pPix(pPix)
	{
		m_pBuffer = NULL;
	}

	~CPixT()
	{
		if (t_bManaged && m_pPix != NULL) Clear();
	}

	CPixT<t_bManaged>& operator=(IPicture* pPix)
	{
		m_pPix = pPix;
		return *this;
	}

////////////////////
// create methods //
////////////////////
	BOOL CreateEmpty(LPPICTDESC lpPictDesc = NULL, BOOL bOwn = FALSE)
	{
		Clear();

		// build the PICTDESC structure if not passed in
		if (lpPictDesc == NULL)
		{
			PICTDESC pdesc;
			pdesc.cbSizeofstruct = sizeof(pdesc);
			pdesc.picType = PICTYPE_NONE;
			lpPictDesc = &pdesc;
		}

		// create the IPicture object
		BOOL bResult = SUCCEEDED(::OleCreatePictureIndirect(lpPictDesc, IID_IPicture, bOwn, (LPVOID*)&m_pPix));

		// clear the pix information structure
		SetPixInfo(NULL);

		return bResult;
	}

	// from bitmap resource
	BOOL CreateFromBitmap(UINT idResource)
	{
		CBitmap bmp;
		bmp.LoadBitmap(idResource);
		return CreateFromBitmap((HBITMAP)bmp.Detach(), NULL, TRUE);
	}

	// from CBitmap
	BOOL CreateFromBitmap(CBitmap* pBitmap, CPalette* pPal, BOOL bTransferOwnership)
	{
		HBITMAP hbm = (HBITMAP)(pBitmap->m_hBitmap);
		HPALETTE hpal = (HPALETTE)(pPal->m_hPalette);
		if (bTransferOwnership)
		{
			if (pBitmap != NULL) pBitmap->Detach();
			if (pPal != NULL) pPal->Detach();
		}
		return CreateFromBitmap(hbm, hpal, bTransferOwnership);
	}

	// from bitmap handle
	BOOL CreateFromBitmap(HBITMAP hbm, HPALETTE hpal, BOOL bTransferOwnership)
	{
		PICTDESC pdesc;
		pdesc.cbSizeofstruct = sizeof(pdesc);
		pdesc.picType = PICTYPE_BITMAP;
		pdesc.bmp.hbitmap = hbm;
		pdesc.bmp.hpal = hpal;
		return CreateEmpty(&pdesc, bTransferOwnership);
	}

	BOOL CreateFromEnhMetafile(HENHMETAFILE hemf, BOOL bTransferOwnership)
	{
		PICTDESC pdesc;
		pdesc.cbSizeofstruct = sizeof(pdesc);
		pdesc.picType = PICTYPE_ENHMETAFILE;
		pdesc.emf.hemf = hemf;
		return CreateEmpty(&pdesc, bTransferOwnership);
	}

	BOOL CreateFromIcon(UINT idResource)
	{
		HICON hIcon = ::LoadIcon(_Module.GetResourceInstance(), MAKEINTRESOURCE(idResource));
		return CreateFromIcon(hIcon, TRUE);
	}

	// works with either icon or cursor
	BOOL CreateFromIcon(HICON hicon, BOOL bTransferOwnership)
	{
		PICTDESC pdesc;
		pdesc.cbSizeofstruct = sizeof(pdesc);
		pdesc.picType = PICTYPE_ICON;
		pdesc.icon.hicon = hicon;
		return CreateEmpty(&pdesc, bTransferOwnership);
	}

	BOOL CreateFromMetafile(HMETAFILE hmf, int xExt, int yExt, BOOL bTransferOwnership)
	{
		PICTDESC pdesc;
		pdesc.cbSizeofstruct = sizeof(pdesc);
		pdesc.picType = PICTYPE_METAFILE;
		pdesc.wmf.hmeta = hmf;
		pdesc.wmf.xExt = xExt;
		pdesc.wmf.yExt = yExt;
		return CreateEmpty(&pdesc, bTransferOwnership);
	}

///////////////////////////
// load IPicture methods //
///////////////////////////
	// load from disk using dispatch
	BOOL LoadFromDispatch(LPTSTR szFilePath)
	{
		if (szFilePath == NULL) return FALSE;
		Clear();
		BOOL bResult = FALSE;
		LPDISPATCH lpDisp = NULL;
		VARIANT varFileName;
		varFileName.vt = VT_BSTR;
		varFileName.bstrVal = CComBSTR(szFilePath);
		bResult = SUCCEEDED(::OleLoadPictureFile(varFileName, &lpDisp));
		if (bResult) SetPictureDispatch((LPPICTUREDISP)lpDisp);
		SetPixInfo(szFilePath);
		return bResult;
	}

	BOOL LoadFromResource(HINSTANCE hResInst, UINT nResID, LPCTSTR szType = _T("IMAGE"))
	{
		Clear();
		BOOL bResult = FALSE;
		
		HRSRC hResInfo = ::FindResource(hResInst, MAKEINTRESOURCE(nResID), szType);
		if (NULL == hResInfo) {
			return FALSE;
		}

		HGLOBAL hImageData = ::LoadResource(hResInst, hResInfo);
		if (NULL == hImageData) {
			return FALSE;
		}

		DWORD cbImage = ::SizeofResource(hResInst, hResInfo);
		if (0 == cbImage) {
			::FreeResource(hImageData);
			return FALSE;
		}

		LPVOID lpImageBuffer = ::LockResource(hImageData);
		if (NULL == lpImageBuffer) {
			::FreeResource(hImageData);
			return FALSE;
		}

		// allocate buffer memory based on file size
		m_pBuffer = ::CoTaskMemAlloc(cbImage);
		if (NULL == m_pBuffer) {
			::FreeResource(hImageData);
			return FALSE;
		}

		::CopyMemory(m_pBuffer, lpImageBuffer, cbImage);

		IStream* pStream = NULL;
		::CreateStreamOnHGlobal(m_pBuffer, FALSE, &pStream);
		bResult = LoadFromIStream(pStream);
		short sPicTyp;
		if (bResult) m_pPix->get_Type(&sPicTyp);
		if (bResult) ::CopyMemory(m_info.piHeader, m_pBuffer, sizeof(m_info.piHeader));

		::CoTaskMemFree(m_pBuffer);
		::FreeResource(hImageData);

		return bResult;
	}

	// load from disk using traditional file handle
	BOOL LoadFromFile(LPCTSTR szFilePath)
	{
		if (szFilePath == NULL) return FALSE;
		Clear();
		BOOL bResult = FALSE;
		HANDLE hFile = ::CreateFile(szFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
	        DWORD dwBytesRead = 0;
			IStream* pStream = NULL;

			// get file size
			DWORD dwFileSize = ::GetFileSize(hFile, NULL);

		    // allocate buffer memory based on file size
			m_pBuffer = ::CoTaskMemAlloc(dwFileSize);
			if (m_pBuffer != NULL)
			{
				// read file and store its data in buffer
				BOOL bRead = ::ReadFile(hFile, m_pBuffer, dwFileSize, &dwBytesRead, NULL);

				// load IPicture from buffer using IStream
		        if (bRead && dwBytesRead > 0)
				{
					::CreateStreamOnHGlobal(m_pBuffer, FALSE, &pStream);
					bResult = LoadFromIStream(pStream);
					short sPicTyp;
					if (bResult) m_pPix->get_Type(&sPicTyp);
					if (bResult) memcpy(m_info.piHeader, m_pBuffer, sizeof(m_info.piHeader));
				}
			}

			::CloseHandle(hFile);
			if (pStream != NULL) pStream->Release();
			::CoTaskMemFree(m_pBuffer);
		}
		SetPixInfo(szFilePath);
		return bResult;
	}

	BOOL LoadFromIStream(IStream* pStream)
	{
		if (pStream == NULL) return FALSE;
		Clear();
		return SUCCEEDED(::OleLoadPicture(pStream, 0, FALSE, IID_IPicture, (LPVOID*)&m_pPix));
	}

	// useful for OLE DB image blobs
	BOOL LoadFromISequentialStream(ISequentialStream* pISeqStream, ULONG& ulLength)
	{
		if (pISeqStream == NULL || ulLength == 0) return FALSE;
		BOOL bResult = FALSE;
		ULONG ulBytesRead = 0;

		// copy bytes to buffer from input sequential stream
		m_pBuffer = ::CoTaskMemAlloc(ulLength);
		pISeqStream->Read(m_pBuffer, ulLength, &ulBytesRead);

		// create an IStream on buffer. Needed for IPicture
		// since ISequentialStream will not work directly
		bResult = LoadFromMemory();

		return bResult;
	}

	BOOL LoadFromMemory()
	{
		if (m_pBuffer == NULL) return FALSE;
		BOOL bResult = FALSE;
		IStream* pIStream = NULL;

		Clear();
		::CreateStreamOnHGlobal(m_pBuffer, FALSE, &pIStream);
		bResult = SUCCEEDED(::OleLoadPicture(pIStream, 0, FALSE, IID_IPicture, (LPVOID*)&m_pPix));
		if (pIStream != NULL) pIStream->Release();
		if (bResult) memcpy(m_info.piHeader, m_pBuffer, sizeof(m_info.piHeader));
		::CoTaskMemFree(m_pBuffer);

		return bResult;
	}

	// simple method of loading from a file or URL but
	// rumored to have resource leaks
	BOOL LoadFromPath(LPCTSTR szFilePath)
	{
		if (szFilePath == NULL) return FALSE;
		Clear();
		BOOL bResult = SUCCEEDED(::OleLoadPicturePath(CComBSTR(szFilePath), NULL, 0, 0, IID_IPicture, (LPVOID *)&m_pPix));
		SetPixInfo(szFilePath);
		return bResult;
	}

///////////////////////////
// save IPicture methods //
///////////////////////////
	// save to disk. OleSavePictureFile may have problems; Use with caution!
	BOOL SaveToDispatch(LPTSTR szFilePath)
	{
		if (szFilePath == NULL || m_pPix == NULL) return FALSE;
		BOOL bResult = SUCCEEDED(::OleSavePictureFile((LPDISPATCH)GetPictureDispatch(), CComBSTR(szFilePath)));
		SetPixInfo(szFilePath);
		return bResult;
	}

	// save to diskfile using traditional file handle
	BOOL SaveToFile(LPCTSTR szFilePath, BOOL fSaveMemCopy = FALSE)
	{
		if (szFilePath == NULL || m_pPix == NULL) return FALSE;

		BOOL bResult = FALSE;
		DWORD dwBytesWritten = 0;

		// copy the picture out to the memory buffer and then to the file
		DWORD dwBytesToWrite = SaveToMemory(fSaveMemCopy);
		if (dwBytesToWrite > 0)
		{
			HANDLE hFile = ::CreateFile(szFilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
			if(hFile != INVALID_HANDLE_VALUE)
			{
				bResult = ::WriteFile(hFile, m_pBuffer, dwBytesToWrite, &dwBytesWritten, FALSE);
				::CloseHandle(hFile);
				::CoTaskMemFree(m_pBuffer);
			}
		}
		SetPixInfo(szFilePath);
		return bResult;
	}

	// fSaveMemCopy - whether or not to save a copy of the picture in memory.
	// Set true for images loaded from clipboard or resource, false for file
	DWORD SaveToMemory(BOOL fSaveMemCopy = FALSE)
	{
		if (m_pPix == NULL) return 0;

		IStream* pIStream = NULL;
		LARGE_INTEGER liOffset = { 0, 0 };
		long lBytesWritten = 0;
		ULONG ulBytesRead = 0;

		// create the IStream and place the picture bytes in it
		::CreateStreamOnHGlobal(NULL, FALSE, &pIStream);
		if (m_pPix->SaveAsFile(pIStream, fSaveMemCopy, &lBytesWritten) == S_OK)
		{
			// reset stream seek position to 0
			if (pIStream->Seek(liOffset, STREAM_SEEK_SET, NULL) == S_OK)
			{
				// initialize the buffer and copy the stream to it
				m_pBuffer = ::CoTaskMemAlloc(lBytesWritten);
				pIStream->Read(m_pBuffer, lBytesWritten, &ulBytesRead);
			}
			if (m_pBuffer != NULL) memcpy(m_info.piHeader, m_pBuffer, sizeof(m_info.piHeader));
		}
		if (pIStream != NULL) pIStream->Release();

		return (DWORD)lBytesWritten;
	}

////////////////////////////////
// IPicture drawing function //
////////////////////////////////
	void Render(CDCHandle dc, const CRect& rcRender)
	{
		if (m_pPix != NULL)
		{
			long hmWidth;
			long hmHeight;

			m_pPix->get_Width(&hmWidth);
			m_pPix->get_Height(&hmHeight);

			m_pPix->Render(dc, rcRender.left, rcRender.top,
				rcRender.Width(), rcRender.Height(), 0, hmHeight-1,
				hmWidth, -hmHeight, NULL);
		}
	}

///////////////////////////////
// IPicture property getters //
///////////////////////////////
	DWORD GetAttributes() const
	{
		DWORD dwAttributes = 0;
		if (m_pPix != NULL) m_pPix->get_Attributes(&dwAttributes);
		return dwAttributes;
	}

	LPCTSTR GetDescription() const
	{
		LPCTSTR strType = NULL;
		short sPixType = GetType();

		switch(sPixType) {
		case PICTYPE_UNINITIALIZED: strType = _T("Uninitialized"); break;
		case PICTYPE_NONE: strType = _T("None"); break;
		case PICTYPE_BITMAP: strType = _T("Windows 3.x Bitmap (BMP)"); break;
		case PICTYPE_METAFILE: strType = _T("Windows Metafile (WMF)"); break;
		case PICTYPE_ICON: strType = _T("Windows Icon (ICO)"); break;
		case PICTYPE_ENHMETAFILE: strType = _T("Enhanced Metafile (EMF)"); break;
		default: strType = _T("Unknown"); break; }

		return strT///////////////////////////////////////////////////////
// Boolean property

class CPropertyBooleanItem : public CPropertyListItem
{
public:
   CPropertyBooleanItem(LPCTSTR pstrName, LPARAM lParam) : CPropertyListItem(pstrName, lParam)
   {
#ifdef IDS_TRUE
      TCHAR szBuffer[32];
      ::LoadString(_Module.GetResourceInstance(), IDS_FALSE, szBuffer, sizeof(szBuffer) / sizeof(TCHAR));
      AddListItem(szBuffer);
      ::LoadString(_Module.GetResourceInstance(), IDS_TRUE, szBuffer, sizeof(szBuffer) / sizeof(TCHAR));
      AddListItem(szBuffer);
#else
      AddListItem(_T("False"));
      AddListItem(_T("True"));
#endif
   }
};


/////////////////////////////////////////////////////////////////////////////
// ListBox Control property

class CPropertyComboItem : public CPropertyItem
{
public:
   CListBox m_ctrl;

   CPropertyComboItem(LPCTSTR pstrName, LPARAM lParam) : 
      CPropertyItem(pstrName, lParam)
   {
   }
   HWND CreateInplaceControl(HWND hWnd, const RECT& rc) 
   {
      ATLASSERT(::IsWindow(m_ctrl));
      // Create window
      CPropertyComboWindow* win = new CPropertyComboWindow();
      ATLASSERT(win);
      RECT rcWin = rc;
      win->m_hWndCombo = m_ctrl;
      win->Create(hWnd, rcWin);
      ATLASSERT(::IsWindow(*win));
      return *win;
   }
   BYTE GetKind() const 
   { 
      return PROPKIND_CONTROL; 
   }
   void DrawValue(PROPERTYDRAWINFO& di) 
   { 
      RECT rc = di.rcItem;
      ::InflateRect(&rc, 0, -1);
      DRAWITEMSTRUCT dis = { 0 };
      dis.hDC = di.hDC;
      dis.hwndItem = m_ctrl;
      dis.CtlID = m_ctrl.GetDlgCtrlID();
      dis.CtlType = ODT_LISTBOX;
      dis.rcItem = rc;
      dis.itemState = ODS_DEFAULT | ODS_COMBOBOXEDIT;
      dis.itemID = m_ctrl.GetCurSel();
      dis.itemData = (int) m_ctrl.GetItemData(dis.itemID);
      ::SendMessage(m_ctrl, OCM_DRAWITEM, dis.CtlID, (LPARAM) &dis);
   }
   BOOL GetValue(VARIANT* pValue) const 
   { 
      CComVariant v = (int) m_ctrl.GetItemData(m_ctrl.GetCurSel());
      return SUCCEEDED( v.Detach(pValue) );
   }
   BOOL SetValue(HWND /*hWnd*/) 
   {      
      int iSel = m_ctrl.GetCurSel();
      CComVariant v = (int) m_ctrl.GetItemData(iSel);
      return SetValue(v); 
   }
   BOOL SetValue(const VARIANT& value)
   {
      ATLASSERT(value.vt==VT_I4);
      for( int i = 0; i < m_ctrl.GetCount(); i++ ) {
         if( m_ctrl.GetItemData(i) == (DWORD_PTR) value.lVal ) {
            m_ctrl.SetCurSel(i);
            return TRUE;
         }
      }
      return FALSE;
   }
};


/////////////////////////////////////////////////////////////////////////////
//
// CProperty creators
//

inline HPROPERTY PropCreateVariant(LPCTSTR pstrName, const VARIANT& vValue, LPARAM lParam = 0)
{
   CPropertyEditItem* prop = NULL;
   ATLTRY( prop = new CPropertyEditItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop ) prop->SetValue(vValue);
   return prop;
}

inline HPROPERTY PropCreateSimple(LPCTSTR pstrName, LPCTSTR pstrValue, LPARAM lParam = 0)
{
   CComVariant vValue = pstrValue;
   return PropCreateVariant(pstrName, vValue, lParam);
}

inline HPROPERTY PropCreateSimple(LPCTSTR pstrName, int iValue, LPARAM lParam = 0)
{
   CComVariant vValue = iValue;
   return PropCreateVariant(pstrName, vValue, lParam);
}

inline HPROPERTY PropCreateSimple(LPCTSTR pstrName, bool bValue, LPARAM lParam = 0)
{
   // NOTE: Converts to integer, since we're using value as an index to dropdown
   CComVariant vValue = (int) bValue & 1;
   CPropertyBooleanItem* prop = NULL;
   ATLTRY( prop = new CPropertyBooleanItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop ) prop->SetValue(vValue);
   return prop;
}

inline HPROPERTY PropCreateFileName(LPCTSTR pstrName, LPCTSTR pstrFileName, LPARAM lParam = 0)
{
   ATLASSERT(!::IsBadStringPtr(pstrFileName,-1));
   CPropertyFileNameItem* prop = NULL;
   ATLTRY( prop = new CPropertyFileNameItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop == NULL ) return NULL;
   CComVariant vValue = pstrFileName;
   prop->SetValue(vValue);
   return prop;
}

inline HPROPERTY PropCreateDate(LPCTSTR pstrName, const SYSTEMTIME stValue, LPARAM lParam = 0)
{
   IProperty* prop = NULL;
   ATLTRY( prop = new CPropertyDateItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop == NULL ) return NULL;
   CComVariant vValue;
   vValue.vt = VT_DATE;
   vValue.date = 0.0; // NOTE: Clears value in case of conversion error below!
   if( stValue.wYear > 0 ) ::SystemTimeToVariantTime( (LPSYSTEMTIME) &stValue, &vValue.date );
   prop->SetValue(vValue);
   return prop;
}

inline HPROPERTY PropCreateList(LPCTSTR pstrName, LPCTSTR* ppList, int iValue = 0, LPARAM lParam = 0)
{
   ATLASSERT(ppList);
   CPropertyListItem* prop = NULL;
   ATLTRY( prop = new CPropertyListItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop && ppList ) {
      prop->SetList(ppList);
      CComVariant vValue = iValue;
      prop->SetValue(vValue);
   }
   return prop;
}

inline HPROPERTY PropCreateComboControl(LPCTSTR pstrName, HWND hWnd, int iValue, LPARAM lParam = 0)
{
   ATLASSERT(::IsWindow(hWnd));
   CPropertyComboItem* prop = NULL;
   ATLTRY( prop = new CPropertyComboItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop ) {
      prop->m_ctrl = hWnd;
      CComVariant vValue = iValue;
      prop->SetValue(vValue);
   }
   return prop;
}

inline HPROPERTY PropCreateCheckButton(LPCTSTR pstrName, bool bValue, LPARAM lParam = 0)
{
   return new CPropertyCheckButtonItem(pstrName, bValue, lParam);
}

inline HPROPERTY PropCreateReadOnlyItem(LPCTSTR pstrName, LPCTSTR pstrValue = _T(""), LPARAM lParam = 0)
{
   ATLASSERT(!::IsBadStringPtr(pstrValue,-1));
   CPropertyItem* prop = NULL;
   ATLTRY( prop = new CPropertyReadOnlyItem(pstrName, lParam) );
   ATLASSERT(prop);
   if( prop ) {
      CComVariant v = pstrValue;
      prop->SetValue(v);
   }
   return prop;
}


#endif // __PROPERTYITEMIMPL__H
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        #ifndef __PROPERTYLIST__H
#define __PROPERTYLIST__H

#pragma once

/////////////////////////////////////////////////////////////////////////////
// CPropertyList - A Property List control
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2001-2003 Bjarke Viksoe.
//   Thanks to Pascal Binggeli for fixing the disabled items.
//   Column resize supplied by Remco Verhoef, thanks.
//   Also thanks to Daniel Bowen, Alex Kamenev and others for fixes.
//
// Add the following macro to the parent's message map:
//   REFLECT_NOTIFICATIONS()
//
// This code may be used in compiled form in any way you desire. This
// file may be redistributed by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name is included. 
//
// This file is provided "as is" with no expressed or implied warranty.
// The author accepts no liability if it causes any damage to you or your
// computer whatsoever. It's free, so don't hassle me about it.
//
// Beware of bugs.
//

#ifndef __cplusplus
  #error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
  #error PropertyList.h requires atlapp.h to be included first
#endif

#ifndef __ATLCTRLS_H__
  #error PropertyList.h requires atlctrls.h to be included first
#endif

#if !((_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400))
  #include <zmouse.h>
#endif //!((_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400))


// Extended List styles
#define PLS_EX_CATEGORIZED     0x00000001
#define PLS_EX_SORTED          0x00000002
#define PLS_EX_XPLOOK          0x00000004
#define PLS_EX_SHOWSELALWAYS   0x00000008
#define PLS_EX_SINGLECLICKEDIT 0x00000010
#define PLS_EX_NOCOLUMNRESIZE  0x00000020

// Include property base class
#include "PropertyItem.h"

// Include property implementations
#include "PropertyItemEditors.h"
#include "Pr