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

		return strType;
	}

	OLE_HANDLE GetHandle() const
	{
		OLE_HANDLE handle = NULL;
		GetHandle(handle);
		return handle;
	}

	void GetHandle(OLE_HANDLE& handle) const
	{
		if (m_pPix != NULL) m_pPix->get_Handle(&handle);
	}

	LPPIXINFO GetPixInformation()
	{
		if (m_pPix == NULL) return NULL;
		m_pPix->get_Type(&m_info.piType);
		m_info.piDescription = GetDescription();
		GetSizeInPixels(m_info.piSize); 
		m_pPix->get_Attributes(&m_info.piAttributes);
		return &m_info;
	}

	void GetSizeInHiMetric(SIZE& hmSize) const
	{
		if (m_pPix == NULL) return;

		m_pPix->get_Width(&hmSize.cx);
		m_pPix->get_Height(&hmSize.cy);
	}

	void GetSizeInPixels(SIZE& size) const
	{
		if (m_pPix == NULL) return;

		SIZE hmSize = { 0, 0 };
		GetSizeInHiMetric(hmSize);
		AtlHiMetricToPixel(&hmSize, &size);
	}

	short GetType() const
	{
		short sPixType = (short)PICTYPE_UNINITIALIZED;
		if (m_pPix != NULL) m_pPix->get_Type(&sPixType);
		return sPixType;
	}

///////////////////////////
// CPix property setters //
///////////////////////////
	void SetPixInfo(LPCTSTR szFilePath)
	{
		memset(m_info.piHeader, 0, sizeof(m_info.piHeader));

		if (szFilePath != NULL)
		{
			HANDLE hFile = ::CreateFile(szFilePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				DWORD dwBytesRead = 0;
				::ReadFile(hFile, m_info.piHeader, sizeof(m_info.piHeader), &dwBytesRead, NULL);
				::CloseHandle(hFile);
			}
		}
	}

////////////////////////////////
// IPicture Dispatch property //
//                            //
// Use with Active X controls //
////////////////////////////////
	LPPICTUREDISP GetPictureDispatch() const
	{
		LPPICTUREDISP pDisp = NULL;
		if (m_pPix == NULL) return NULL;
		if (SUCCEEDED(m_pPix->QueryInterface(IID_IPictureDisp, (LPVOID*)&pDisp)))
			ATLASSERT(pDisp != NULL);
		return pDisp;
	}

	void SetPictureDispatch(LPPICTUREDISP pDisp)
	{
		LPPICTURE pPix = NULL;
		if (pDisp == NULL) return;
		if (m_pPix != NULL) Clear();
		if (SUCCEEDED(pDisp->QueryInterface(IID_IPicture, (LPVOID*)&pPix)))
		{
			ATLASSERT(pPix != NULL);
			m_pPix = pPix;
		}
		else m_pPix = NULL;
	}

///////////
// Tests //
///////////
	inline bool IsNull() const { return (m_pPix == NULL); }
	inline bool IsBitmap() const { return GetType() == PICTYPE_BITMAP; }
	inline bool IsMetafile() const { return GetType() == PICTYPE_METAFILE; }
	inline bool IsIcon() const { return GetType() == PICTYPE_ICON; }
	inline bool IsEnhMetafile() const { return GetType() == PICTYPE_ENHMETAFILE; }

///////////////////////////
// Miscellaneous helpers //
///////////////////////////
	inline void Clear()
	{
		SafeRelease((LPUNKNOWN*)&m_pPix);
	}

	DWORD SafeRelease(LPUNKNOWN* lplpUnknown)
	{
		ATLASSERT(lplpUnknown != NULL);
		if (*lplpUnknown != NULL)
		{
			DWORD dwRef = (*lplpUnknown)->Release();
			*lplpUnknown = NULL;
			return dwRef;
		}
		return 0;
	}
};

typedef CPixT<false> CPixHandle;
typedef CPixT<true> CPix;

/////////////////////////////////////////
// File Headers                        //
// Naming convention: <type>FILEHEADER //
/////////////////////////////////////////

////////////////////////////////////////////////////////
/* BITMAPFILEHEADER (14 bytes) is defined in wingdi.h */
////////////////////////////////////////////////////////

////////////////////////////////
/* CURSORFILEHEADER (6 bytes) */
////////////////////////////////
#include <pshpack2.h>
typedef struct tagCURSORFILEHEADER {
	WORD cfReserved1;
	WORD cfType;  // 2 = cursor
	WORD cfCount; // number of cursors
} CURSORFILEHEADER, FAR *LPCURSORFILEHEADER;
#include <poppack.h>

//////////////////////////////////////////
/* ENHMETAFILEHEADER (80 bytes)         */
/* ENHMETAHEADER is defined in wingdi.h */
//////////////////////////////////////////
typedef struct tagENHMETAHEADER *LPENHMETAFILEHEADER;

//////////////////////////////
/* ICONFILEHEADER (6 bytes) */
//////////////////////////////
#include <pshpack2.h>
typedef struct tagICONFILEHEADER {
	WORD ifReserved1;
	WORD ifType;  // 1 = icon
	WORD ifCount; // number of icons
} ICONFILEHEADER, FAR *LPICONFILEHEADER;
#include <poppack.h>

///////////////////////////////
/* JPEGFILEHEADER (20 bytes) */
///////////////////////////////
/* Length of APP0 block (2 bytes)
 * Block ID (4 bytes - ASCII "JFIF")
 * Zero byte (1 byte to terminate the ID string)
 * Version Major, Minor (2 bytes - major first)
 * Units (1 byte - 0x00 = none, 0x01 = inch, 0x02 = cm)
 * Xdpu (2 bytes - dots per unit horizontal)
 * Ydpu (2 bytes - dots per unit vertical)
 * Thumbnail X size (1 byte)
 * Thumbnail Y size (1 byte) */
#include <pshpack1.h>
typedef struct tagJPEGFILEHEADER {
// 0xE0FF APP0
// 0xD8FF SOI (start of image, just after 0xE0FF)
	DWORD jfID;           // 0xE0FFD8FF
	WORD jfHdrLength;     // 0x1000 (16 bytes including this)
	BYTE jfSignature1;    // 'J'
	BYTE jfSignature2;    // 'F'
	BYTE jfSignature3;    // 'I'
	BYTE jfSignature4;    // 'F'
	BYTE jfSigTerminator; // '\0'
	BYTE jfMajorVersion;
	BYTE jfMinorVersion;
	BYTE jfUnits;
	WORD jfXdpu;
	WORD jfYdpu;
	BYTE jfThumbnailSizeX;
	BYTE jfThumbnailSizeY;
// next is 0xDBFF Define quantization Table(s)
// ...
// 0xD9FF EOI (end of image / end of file)
} JPEGFILEHEADER, FAR *LPJPEGFILEHEADER;
#include <poppack.h>

//////////////////////////////////////////////////////////
/* METAFILEHEADER (22 bytes) placeable metafile header. */
/* Metafiles may also start with a METAINFOHEADER only  */
/* or with a METAFILEPICT clipboard header (see below)  */
//////////////////////////////////////////////////////////
#include <pshpack2.h>
typedef struct tagMETAFILEHEADER {
	DWORD mfKey;      // Magic number (always 0x9AC6CDD7)
	WORD mfHandle;    // Metafile HANDLE number (always 0)
	WORD mfLeft;      // Left coordinate in metafile units (twips)
	WORD mfTop;       // Top coordinate in metafile units
	WORD mfRight;     // Right coordinate in metafile units
	WORD mfBottom;    // Bottom coordinate in metafile units
	WORD mfInch;      // Number of metafile units per inch.
					  // Unscaled = 1440; 720 = 2:1; 2880 = 1:2; etc.
	DWORD mfReserved; // Reserved (always 0)
	WORD mfChecksum;  // Checksum value for previous 10 WORDs
} METAFILEHEADER, FAR *LPMETAFILEHEADER;
#include <poppack.h>

/////////////////////////////////////////
/* METACLIPHEADER (16 bytes)           */
/* METAFILEPICT is defined in wingdi.h */
/////////////////////////////////////////
/* 32 bit clipboard metafile header (CLP file extension) */
// LONG  mm;   // MapMode - Units used to playback metafile
		// MM_TEXT - One pixel; MM_LOMETRIC - 0.1 millimeter;
		// MM_HIMETRIC - 0.01 millimeter; MM_LOENGLISH - 0.01 inch;
		// MM_HIENGLISH - 0.001 inch; MM_TWIPS - 1/1440th of an inch;
		// MM_ISOTROPIC - Application specific (aspect ratio preserved) 
		// MM_ANISOTROPIC - Application specific (aspect ratio not preserved) 
// LONG  xExt; // Width of the metafile
// LONG  yExt; // Height of the metafile
// DWORD hMF;  // Handle to the metafile in memory
typedef struct tagMETAFILEPICT METACLIPHEADER;
typedef struct tagMETACLIPHEADER FAR *LPMETACLIPHEADER;



/////////////////////////////////////////
// Information Headers                 //
// Naming convention: <type>INFOHEADER //
/////////////////////////////////////////

////////////////////////////////////////////////////////
/* BITMAPINFOHEADER (56 bytes) is defined in wingdi.h */
////////////////////////////////////////////////////////

/////////////////////////////////
/* CURSORINFOHEADER (16 bytes) */
/////////////////////////////////
typedef struct tagCURSORINFOHEADER {
	BYTE ciWidth;
	BYTE ciHeight;
	BYTE ciReserved1;  // color count is obtained from bitmap header (?)
	BYTE ciReserved2;
	WORD ciXHotSpot;
	WORD ciYHotSpot;
	DWORD ciSizeImage; // in bytes
	DWORD ciOffset;    // byte offset to bitmap header
} CURSORINFOHEADER, FAR *LPCURSORINFOHEADER;

//////////////////////////////////
/* ENHMETAINFOHEADER (0 bytes)  */
/* Info is in ENHMETAFILEHEADER */
//////////////////////////////////

///////////////////////////////
/* ICONINFOHEADER (16 bytes) */
///////////////////////////////
typedef struct tagICONINFOHEADER {
	BYTE iiWidth;
	BYTE iiHeight;
	BYTE iiColor;      // 2 (mono), 16, or 0 = 256
	BYTE iiReserved1;
	WORD iiPlanes;
	WORD iiBitCount;   // 1, 4, 8 (?)
	DWORD iiSizeImage; // in bytes
	DWORD iiOffset;    // byte offset to bitmap header
} ICONINFOHEADER, FAR *LPICONINFOHEADER;

/////////////////////////////////////
/* JPEGINFOHEADER (10 bytes)       */
/* Original was defined in mmreg.h */
/////////////////////////////////////
#if defined tagJPEGINFOHEADER
#undef tagJPEGINFOHEADER
#endif
#include <pshpack1.h>
typedef struct tagJPEGINFOHEADER {
	// starts after SOF0 marker (0xFFC0)
	WORD jiHdrLength;      // (3 * jiNumComponents) + 8
	BYTE jiPrecision;      // bits of precision in image data
	WORD jiHeight;         // height, width <= 65535L
	WORD jiWidth;
	BYTE jiNumComponents;
	BYTE jiComponentID;
	BYTE jiSamplingFactor; // HSamp << 4 + VSamp
} JPEGINFOHEADER, FAR *LPJPEGINFOHEADER;
#include <poppack.h>

///////////////////////////////////////
/* METAINFOHEADER (18 bytes)         */
/* METAHEADER is defined in wingdi.h */
///////////////////////////////////////
// WORD  mtType;         // Type of metafile (0=memory, 1=disk)
// WORD  mtHeaderSize;   // Size of header in WORDS (always 9)
// WORD  mtVersion;      // Version of Microsoft Windows used
// DWORD mtSize;         // Total size of the metafile in WORDs
// WORD  mtNoObjects;    // Number of objects in the file
// DWORD mtMaxRecord;    // The size of largest record in WORDs
// WORD  mtNoParameters; // Not Used (always 0)
#include <pshpack2.h>
typedef struct tagMETAHEADER METAINFOHEADER;
typedef struct tagMETAHEADER UNALIGNED FAR *LPMETAINFOHEADER;
#include <poppack.h>

///////////////////////////////////////////////////////////////////

#endif // __PIX_H__
