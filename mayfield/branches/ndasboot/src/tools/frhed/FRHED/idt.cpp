#include "precomp.h"
#include "idt.h"
#include "hexwnd.h"
#include "resource.h"

/*The #ifndef __CYGWIN__s are there because cygwin/mingw doesn't yet have
certain APIs in their import libraries. Specifically _wremove, _wopen & GetEnhMetaFileBits.*/

template<class T>void swap(T& x, T& y){
	T temp = x;
	x = y;
	y = temp;
}

//Members
CDropTarget::CDropTarget( bool delself, CDropTarget** p )
{
#ifdef _DEBUG
	printf("IDropTarget::IDropTarget\n");
	if( !p ) printf("IDropTarget constructed without pthis\n");
#endif //_DEBUG
	m_cRefCount = 0;
	deleteself = delself;
	pthis = p;
}

CDropTarget::~CDropTarget()
{
#ifdef _DEBUG
	printf("IDropTarget::~IDropTarget\n");
	if( m_cRefCount != 0 )
		printf("Deleting %s too early 0x%x.m_cRefCount = %d\n", "IDropTarget", this, m_cRefCount);
#endif //_DEBUG
	if( pthis ) *pthis = NULL;
}


//IUnknown methods
STDMETHODIMP CDropTarget::QueryInterface( REFIID iid, void** ppvObject )
{
#ifdef _DEBUG
	printf("IDropTarget::QueryInterface\n");
#endif //_DEBUG

	*ppvObject = NULL;

	if ( iid == IID_IUnknown ) *ppvObject = (IUnknown*)this;
	else if ( iid == IID_IDropTarget ) *ppvObject = (IDropTarget*)this;

	if(*ppvObject){
		((IUnknown*)*ppvObject)->AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CDropTarget::AddRef( void )
{
#ifdef _DEBUG
	printf("IDropTarget::AddRef\n");
#endif //_DEBUG
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CDropTarget::Release( void )
{
#ifdef _DEBUG
	printf("IDropTarget::Release\n");
#endif //_DEBUG
	if( --m_cRefCount == 0 && deleteself ) delete this;
	return m_cRefCount;
}


//IDropTarget methods
STDMETHODIMP CDropTarget::DragEnter( IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect )
{
#ifdef _DEBUG
	printf("IDropTarget::DragEnter\n");
#endif //_DEBUG
	pDataObj = pDataObject;
	pDataObject->AddRef();
	hdrop_present = false;
	if( hexwnd.prefer_CF_HDROP ){
		FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, 0xffffffff };
		if( S_OK == pDataObject->QueryGetData( &fe ) ){
			hdrop_present = true;
			*pdwEffect = DROPEFFECT_NONE;
			return S_OK;
		} else {
			STGMEDIUM stm;
			if( S_OK == pDataObject->GetData( &fe, &stm ) ){
				hdrop_present = true;
				ReleaseStgMedium( &stm );
				*pdwEffect = DROPEFFECT_NONE;
				return S_OK;
			}
		}
	}

	CreateCaret( hexwnd.hwnd, (HBITMAP)1, 2, hexwnd.cyChar );

	return DragOver( grfKeyState, pt, pdwEffect );
}

STDMETHODIMP CDropTarget::DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect )
{
#ifdef _DEBUG
	printf("IDropTarget::DragOver\n");
#endif //_DEBUG

	LastKeyState = grfKeyState;

	DWORD dwOKEffects = *pdwEffect;

	//We only move or copy if we are not read-only
	//And only if the source supports it
	//but in read-only mode we accept drag-drop loading (CF_HDROP)
	//this is handled by OLE it seems if the window is DragAcceptFiles( hwnd, TRUE )
	if( hdrop_present || hexwnd.bReadOnly ){
		*pdwEffect = DROPEFFECT_NONE;
	}
	else if( grfKeyState & MK_CONTROL )
		*pdwEffect = dwOKEffects & DROPEFFECT_COPY ? DROPEFFECT_COPY : ( dwOKEffects & DROPEFFECT_MOVE ? DROPEFFECT_MOVE : DROPEFFECT_NONE );
	else
		*pdwEffect = dwOKEffects & DROPEFFECT_MOVE ? DROPEFFECT_MOVE : ( dwOKEffects & DROPEFFECT_COPY ? DROPEFFECT_COPY : DROPEFFECT_NONE );

	POINT p = {pt.x, pt.y};
	ScreenToClient(hexwnd.hwnd,&p);
	hexwnd.iMouseX = p.x;
	hexwnd.iMouseY = p.y;

	if( *pdwEffect == DROPEFFECT_NONE ) HideCaret( hexwnd.hwnd );
	else hexwnd.set_drag_caret( p.x, p.y, *pdwEffect == DROPEFFECT_COPY, !!(grfKeyState & MK_SHIFT) );

	hexwnd.dragging = true;

	hexwnd.fix_scroll_timers(p.x,p.y);

	return S_OK;
}

STDMETHODIMP CDropTarget::DragLeave ( void )
{
#ifdef _DEBUG
	printf("IDropTarget::DragLeave\n");
#endif //_DEBUG

	//This is the lesser of two evils
	//1. recreate the editing caret whenever the mouse leaves the client area
	//2. rather than have the caret disappear whenever the mouse leaves the client area (until set_focus is called)
	//#2 would just call CreateCaret
	CreateCaret( hexwnd.hwnd, NULL, hexwnd.cxChar, hexwnd.cyChar );
	hexwnd.set_caret_pos ();
	if( GetFocus() == hexwnd.hwnd )
		ShowCaret( hexwnd.hwnd );

	hexwnd.kill_scroll_timers();

	/*lbuttonup will reset this
	but we need it on for the benefit
	of keydown when the user presses esc*/
	hexwnd.dragging = true;
	if( pDataObj ){
		pDataObj->Release();
		pDataObj = NULL;
	}

	return S_OK;
}

STDMETHODIMP CDropTarget::Drop( IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect )
{
#ifdef _DEBUG
	printf("IDropTarget::Drop\n");
#endif //_DEBUG

	DWORD dwOKEffects = *pdwEffect;
	{
		//We do this because grfKeyState will always have the mouse button used off
		DWORD temp = LastKeyState;
		//Get the position of the drop
		DragOver( grfKeyState, pt, pdwEffect );
		LastKeyState = temp;
	}
	//Remove the effects
	pDataObj = NULL;
	DragLeave();

	bool copying = *pdwEffect & DROPEFFECT_COPY ? true : false ;

	hexwnd.bDroppedHere = TRUE;

	if( /*Internal data*/ hexwnd.bMoving ){
		if( ((LastKeyState|grfKeyState)&(MK_MBUTTON|MK_RBUTTON)) || hexwnd.always_pick_move_copy ){
			HMENU hm = CreatePopupMenu();
			InsertMenu( hm, 0, MF_BYPOSITION|MF_STRING, 1, "Move" );
			InsertMenu( hm, 1, MF_BYPOSITION|MF_STRING, 2, "Copy" );
			InsertMenu( hm, 2, MF_BYPOSITION|MF_SEPARATOR, 0, 0 );
			InsertMenu( hm, 3, MF_BYPOSITION|MF_STRING, 0, "Cancel" );
			BOOL mi = TrackPopupMenuEx( hm, TPM_NONOTIFY|TPM_RIGHTBUTTON|TPM_RETURNCMD, pt.x, pt.y, hexwnd.hwnd, NULL );
			DestroyMenu( hm );
			if( mi == 0 ){ pDataObject->Release(); *pdwEffect = DROPEFFECT_NONE; return S_OK;  }
			copying = ( mi == 1 ? false : true );
		}
		iMove1stEnd = hexwnd.iStartOfSelection;
		iMove2ndEndorLen = hexwnd.iEndOfSelection;
		if( iMove1stEnd > iMove2ndEndorLen ) swap( iMove1stEnd, iMove2ndEndorLen );
		if( !copying && hexwnd.new_pos > iMove2ndEndorLen ) hexwnd.new_pos += iMove1stEnd - iMove2ndEndorLen - 1;
		iMovePos = hexwnd.new_pos;
		iMoveOpTyp = copying ? OPTYP_COPY : OPTYP_MOVE;
		if( /*Overwrite*/ grfKeyState&MK_SHIFT ){
			int len = iMove2ndEndorLen - iMove1stEnd + 1;
			if( copying ){
				//Just [realloc &] memmove
				int upper = 1+hexwnd.DataArray.GetUpperBound();
				if( /*Need more space*/ iMovePos+len > upper ){
					if( hexwnd.DataArray.SetSize( iMovePos+len ) ){
						hexwnd.DataArray.ExpandToSize();
						memmove(&hexwnd.DataArray[iMovePos],&hexwnd.DataArray[iMove1stEnd],len);
					}
				} else /*Enough space*/ {
					memmove(&hexwnd.DataArray[iMovePos],&hexwnd.DataArray[iMove1stEnd],len);
				}
			} else /*Moving*/ {
				if( /*Forward*/ iMovePos > iMove1stEnd ){
					hexwnd.CMD_move_copy( 1, 0 );
					hexwnd.DataArray.RemoveAt(iMovePos+len,len);
				} else /*Backward*/ {
					memmove(&hexwnd.DataArray[iMovePos],&hexwnd.DataArray[iMove1stEnd],len);
					hexwnd.DataArray.RemoveAt((iMovePos-iMove1stEnd>=len?iMove1stEnd:iMovePos+len),len);
				}
			}
			hexwnd.iStartOfSelection = iMovePos;
			hexwnd.iEndOfSelection = iMovePos+len-1;
			hexwnd.m_iFileChanged = hexwnd.bFilestatusChanged = hexwnd.bSelected = TRUE;
			hexwnd.update_for_new_datasize();
		} else /*Insert*/ hexwnd.CMD_move_copy( 1 );
	} else /*External data*/ {

		HRESULT err = E_UNEXPECTED;

		//Get the formats enumerator
		IEnumFORMATETC* iefe = NULL;
		pDataObject->EnumFormatEtc( DATADIR_GET, &iefe );
		if( !iefe ){
#ifdef _DEBUG
			printf("Unable to create a drag-drop data enumerator\n");
#endif //_DEBUG
			goto ERR;
		}
		try{
			iefe->Reset();
		}
		catch(...){
			goto ERR;
		}

		//Get the available formats
		FORMATETC* fel;fel = NULL;
		UINT numfe;numfe = 0;
		for(;;){
			void* temp = realloc( fel, ( numfe + 1 ) * sizeof(FORMATETC) );
			if( temp != NULL ){
				fel = (FORMATETC*) temp;
				temp = NULL;
				int r;
				r = iefe->Next( 1, &fel[numfe], NULL);
				if( r != S_OK ) break;//No more formats
				numfe++;
			} else if( fel == NULL ) {
				//We only go here if nothing could be allocated
#ifdef _DEBUG
				printf("Not enough memory for the drag-drop format list\n");
#endif //_DEBUG
				goto ERR_ENUM;
			}
		}

		/*Check which format should be inserted according to user preferences*/
		bool NeedToChooseFormat;NeedToChooseFormat = true;
		int IndexOfDataToInsert;IndexOfDataToInsert = -1;
		if( numfe == 0 ){
			MessageBox( hexwnd.hwnd, "No data to insert", "Drag-drop", MB_OK );
			err = S_OK;
			*pdwEffect = DROPEFFECT_NONE;
			goto ERR_ENUM;
		} else if( numfe == 1 ){
			IndexOfDataToInsert = 0;
		} else if( hexwnd.prefer_CF_HDROP ){
			for( UINT i=0; i < numfe; i++ ){
				if( fel[i].cfFormat == CF_HDROP ){
					//Return no effect & let shell32 handle it
					hexwnd.dontdrop = false;
					err = S_OK;
					*pdwEffect = DROPEFFECT_NONE;
					goto ERR_ENUM;
				}
			}
		} else if( hexwnd.prefer_CF_BINARYDATA ){
			for( UINT i=0; i < numfe; i++ ){
				if( fel[i].cfFormat == CF_BINARYDATA ){
					NeedToChooseFormat = false;
					IndexOfDataToInsert = i;
					break;
				}
			}
		} else if( hexwnd.prefer_CF_TEXT ){
			for( UINT i=0; i < numfe; i++ ){
				if( fel[i].cfFormat == CF_TEXT ){
					NeedToChooseFormat = false;
					IndexOfDataToInsert = i;
					break;
				}
			}
		}

		UINT* formats;formats = NULL;
		UINT numformats;numformats = 0;
		bool NeedToChooseMoveorCopy;
		NeedToChooseMoveorCopy = ( ( ( (LastKeyState|grfKeyState) & ( MK_MBUTTON | MK_RBUTTON ) ) ) || hexwnd.always_pick_move_copy ) ? true : false;
		if( NeedToChooseFormat == false && NeedToChooseMoveorCopy == true ){
			HMENU hm = CreatePopupMenu();
			InsertMenu( hm, 0, MF_BYPOSITION|MF_STRING, 1, "Move" );
			InsertMenu( hm, 1, MF_BYPOSITION|MF_STRING, 2, "Copy" );
			InsertMenu( hm, 2, MF_BYPOSITION|MF_SEPARATOR, 0, 0 );
			InsertMenu( hm, 3, MF_BYPOSITION|MF_STRING, 0, "Cancel" );
			BOOL mi = TrackPopupMenuEx( hm, TPM_NONOTIFY|TPM_RIGHTBUTTON|TPM_RETURNCMD, pt.x, pt.y, hexwnd.hwnd, NULL );
			DestroyMenu( hm );
			if( mi == 0 ){ hexwnd.dontdrop = true; err = S_OK; *pdwEffect = DROPEFFECT_NONE; goto ERR_ENUM; }
			copying = ( mi == 1 ? false : true );
		} else if( NeedToChooseFormat ) {
			DROPPARAMS params;
			params.allowable_effects = dwOKEffects & ( DROPEFFECT_COPY | DROPEFFECT_MOVE );
			params.effect = copying;
			params.formatetcs = fel;
			params.numformatetcs = numfe;
			params.formats = NULL;
			params.numformats = 0;
			int ret = DialogBoxParam( hexwnd.hInstance, MAKEINTRESOURCE(IDD_DRAGDROP), hexwnd.hwnd, (DLGPROC) DragDropDlgProc, (LPARAM)&params );
			if( ret < 0 ){//An error occured or the user canceled the operation
				hexwnd.dontdrop = true;
				err = S_OK;
				*pdwEffect = DROPEFFECT_NONE;
				goto ERR_ENUM;
			}
			numformats = params.numformats;
			formats = params.formats;
			copying = params.effect;
		}

		if( IndexOfDataToInsert >= 0 && formats == NULL){
			formats = (UINT*)&IndexOfDataToInsert;
			numformats = 1;
		}


		FORMATETC fe;
		STGMEDIUM stm;
		int ret;
		size_t totallen;totallen = 0;
		BYTE* data;data = NULL;
		bool gotdata;gotdata = false;
		//for each selected format
		UINT i;
		for(i = 0;i<numformats;i++){
			fe = fel[formats[i]];
			/*It is important that when debugging (with M$VC at least) you do not step __into__ the below GetData call
			  If you do the app providing the data source will die & GetData will return OLE_E_NOTRUNNING or E_FAIL
			  The solution is to step over the call
			  It is also possible that a debugger will be opened & attach itself to the data provider
			*/
			if ( (ret = pDataObject->GetData( &fe, &stm )) == S_OK )
			{
				//Get len
				size_t len = 0;
				switch( stm.tymed ){
					case TYMED_HGLOBAL: len = GlobalSize( stm.hGlobal ); break;
#ifndef __CYGWIN__
					case TYMED_FILE:{
						int fh = _wopen( stm.lpszFileName, _O_BINARY | _O_RDONLY );
						if( fh != -1 ){
							len = _filelength( fh );
							if( len == (size_t)-1 ) len = 0;
							_close( fh );
						}
					} break;
#endif //__CYGWIN__
					case TYMED_ISTREAM:{
						STATSTG stat;
						if( S_OK == stm.pstm->Stat( &stat, STATFLAG_NONAME ) ) len = (size_t)stat.cbSize.LowPart;
					} break;
					//This case is going to be a bitch to implement so it can wait for a while
					//It will need to be a recursive method that stores the STATSTG structures (+ the name), contents/the bytes of data in streams/property sets
					case TYMED_ISTORAGE:{ MessageBox( hexwnd.hwnd, "TYMED_ISTORAGE is not yet supported for drag-drop.\nPlease don't hesitate to write a patch & send in a diff.", "Drag-drop", MB_OK ); } break;//IStorage*
					case TYMED_GDI:{
						len = GetObject( stm.hBitmap, 0, NULL );
						if( len ){
							DIBSECTION t;
							GetObject( stm.hBitmap, len, &t );
							len += t.dsBm.bmHeight*t.dsBm.bmWidthBytes*t.dsBm.bmPlanes;
						}
					} break;//HBITMAP
					case TYMED_MFPICT:{
						len = GlobalSize( stm.hMetaFilePict );
						METAFILEPICT*pMFP=(METAFILEPICT*)GlobalLock( stm.hMetaFilePict );
						if( pMFP ){
							len += GetMetaFileBitsEx( pMFP->hMF, 0, NULL );
							GlobalUnlock( stm.hMetaFilePict );
						}
					} break;//HMETAFILE
#ifndef __CYGWIN__
					case TYMED_ENHMF:{
						len = GetEnhMetaFileHeader( stm.hEnhMetaFile, 0, NULL );
						DWORD n = GetEnhMetaFileDescriptionW( stm.hEnhMetaFile, 0, NULL );
						if( n && n != GDI_ERROR ) len += sizeof(WCHAR)*n;
						len += GetEnhMetaFileBits( stm.hEnhMetaFile, 0, NULL );
						n = GetEnhMetaFilePaletteEntries( stm.hEnhMetaFile, 0, NULL );
						if( n && n != GDI_ERROR ) len += sizeof(LOGPALETTE)+(n-1)*sizeof(PALETTEENTRY);
					} break;//HENHMETAFILE
#endif //__CYGWIN__
					//case TYMED_NULL:break;
				}
				if( !len ) continue;

				/*Malloc
				We do this so that if the data access fails we only need free(data)
				and don't need to mess around with the DataArray.
				Perhaps in the future the DataArray can support undoable actions.*/
				BYTE* t = (BYTE*)realloc(data, len);
				if( !t ) continue;
				data = t;
				memset( data, 0, len );

				//Get data
				switch( stm.tymed ){
					case TYMED_HGLOBAL:{
						LPVOID pmem = GlobalLock( stm.hGlobal );
						if( pmem ){
							try{ memcpy( data, pmem, len ); }
							catch(...){}
							gotdata = true;
							GlobalUnlock( stm.hGlobal );
						}
					} break;
#ifndef __CYGWIN__
					case TYMED_FILE:{
						int fh = _wopen( stm.lpszFileName, _O_BINARY | _O_RDONLY );
						if( fh != -1 ){
							if( 0 < _read( fh, data, len ) ) gotdata = true;
							_close( fh );
						}
					} break;
#endif //__CYGWIN__
					case TYMED_ISTREAM:{
						LARGE_INTEGER zero = { 0 };
						ULARGE_INTEGER pos;
						if( S_OK == stm.pstm->Seek( zero, STREAM_SEEK_CUR, &pos ) ){
							stm.pstm->Seek( zero, STREAM_SEEK_SET, NULL );
							if( S_OK == stm.pstm->Read( data, len, NULL ) ) gotdata = true;
							stm.pstm->Seek( *(LARGE_INTEGER*)&pos, STREAM_SEEK_SET, NULL );
						}
					} break;
					//This case is going to be a bitch to implement so it can wait for a while
					//It will need to be a recursive method that stores the STATSTG structures (+ the name), contents/the bytes of data in streams/property sets
					case TYMED_ISTORAGE:{ MessageBox( hexwnd.hwnd, "TYMED_ISTORAGE is not yet supported for drag-drop.\nPlease don't hesitate to write a patch & send in a diff.", "Drag-drop", MB_OK ); goto ERR_ENUM; } break;//IStorage*
					case TYMED_GDI:{
						int l = GetObject( stm.hBitmap, len, data );
						if( l ){
							BITMAP* bm = (BITMAP*)data;
							if( bm->bmBits ) memcpy( &data[l], bm->bmBits, len-l );
							else GetBitmapBits( stm.hBitmap, len-l, &data[l] );
							gotdata = true;
						}
					} break;//HBITMAP
					case TYMED_MFPICT:{
						METAFILEPICT*pMFP=(METAFILEPICT*)GlobalLock( stm.hMetaFilePict );
						if( pMFP ){
							try{
								memcpy( data, pMFP, sizeof(*pMFP) );
								GetMetaFileBitsEx( pMFP->hMF, len - sizeof(*pMFP), &data[sizeof(*pMFP)] );
								GlobalUnlock( stm.hMetaFilePict );
								gotdata = true;
							}
							catch(...){}
						}
					} break;//HMETAFILE
#ifndef __CYGWIN__
					case TYMED_ENHMF:{
						DWORD i = 0, n = 0, l = len;
						n = GetEnhMetaFileHeader( stm.hEnhMetaFile, l, (ENHMETAHEADER*)&data[i] );
						l -= n; i += n;
						n = GetEnhMetaFileDescriptionW( stm.hEnhMetaFile, l/sizeof(WCHAR), (LPWSTR)&data[i] );
						if( n && n != GDI_ERROR ){
							n *= sizeof(WCHAR);l -= n; i += n;
						}
						n = GetEnhMetaFileBits( stm.hEnhMetaFile, l, &data[i] );
						l -= n; i += n;
						n = GetEnhMetaFilePaletteEntries( stm.hEnhMetaFile, 0, NULL );
						if( n && n != GDI_ERROR ){
							LOGPALETTE* lp = (LOGPALETTE*)&data[i];
							lp->palVersion = 0x300;
							lp->palNumEntries = (USHORT)n;
							l -=sizeof(lp->palVersion)+sizeof(lp->palNumEntries);
							n = GetEnhMetaFilePaletteEntries( stm.hEnhMetaFile, l/sizeof(PALETTEENTRY), &lp->palPalEntry[0] );
							i += n*sizeof(PALETTEENTRY);
						}
						if( i ) gotdata = true;
					} break;//HENHMETAFILE
#endif //__CYGWIN__
					//case TYMED_NULL:break;
				}

				ReleaseStgMedium( &stm );

				if(gotdata){
					BYTE* DataToInsert = data;
					if( fe.cfFormat == CF_BINARYDATA ){
						len = *(DWORD*)data;
						DataToInsert += 4;
					} else if( fe.cfFormat == CF_TEXT || fe.cfFormat == CF_OEMTEXT ){
						try { len = strlen( (char*)data ); } catch(...){}
					} else if( fe.cfFormat == CF_UNICODETEXT ){
						try { len = sizeof(wchar_t)*wcslen( (wchar_t*)data ); } catch(...){}
					}

					//Insert/overwrite data into DataArray
					if( /*Overwite*/ grfKeyState&MK_SHIFT ){
						DWORD upper = 1+hexwnd.DataArray.GetUpperBound();
						if( /*Need more space*/ hexwnd.new_pos+len > upper ){
							if( hexwnd.DataArray.SetSize( hexwnd.new_pos+totallen+len ) ){
								hexwnd.DataArray.ExpandToSize();
								memcpy(&hexwnd.DataArray[hexwnd.new_pos+(int)totallen],DataToInsert,len);
								gotdata = true;
								totallen += len;
							}
						} else /*Enough space*/ {
							memcpy(&hexwnd.DataArray[hexwnd.new_pos+(int)totallen],DataToInsert,len);
							gotdata = true;
							totallen += len;
						}
					} else /*Insert*/ if( 0 != hexwnd.DataArray.InsertAtGrow( hexwnd.new_pos+totallen, DataToInsert, 0, len ) ){
							gotdata = true;
							totallen += len;
					}
				}

			}

		} //for each selected format

		//Release the data
		free( data ); data = NULL;
		if( IndexOfDataToInsert < 0 ){
			free( formats ); formats = NULL;
		}

		if(gotdata){
			hexwnd.iStartOfSelection = hexwnd.new_pos;
			hexwnd.iEndOfSelection = hexwnd.new_pos + totallen - 1;
			hexwnd.m_iFileChanged = hexwnd.bFilestatusChanged = hexwnd.bSelected = TRUE;
			hexwnd.update_for_new_datasize();
		}

		*pdwEffect = copying  ? DROPEFFECT_COPY : DROPEFFECT_MOVE;

		err = S_OK;

ERR_ENUM:
		iefe->Release();
		free( fel );
ERR:
		pDataObject->Release();
		return err;
	}
	pDataObject->Release();

	return S_OK;
}


