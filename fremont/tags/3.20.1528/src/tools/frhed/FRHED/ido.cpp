#include "precomp.h"
#include "ido.h"
#include "hexwnd.h"
#include <shlwapi.h>

/*The #ifndef __CYGWIN__s are there because cygwin/mingw doesn't yet have
certain APIs in their import libraries. Specifically _wremove, _wopen & GetEnhMetaFileBits.*/

//Prototypes
STGMEDIUM* TransferMedium( STGMEDIUM* dest, STGMEDIUM* source, bool allowresize = true, bool cpysmlr = false );


//CDataObject
//Members
CDataObject::CDataObject( bool delself, CDataObject** p )//When using C++ new need to call like so someptr = new CDataObject(1,someptr);
{
#ifdef _DEBUG
	printf("IDataObject::IDataObject\n");
#endif //_DEBUG
	m_cRefCount = 0;
	deleteself = delself;
	pthis = p;

	allowSetData = true;
	data = NULL;
	enums = NULL;
	numdata = numenums = 0;
}

CDataObject::~CDataObject( void )
{
#ifdef _DEBUG
	printf("IDataObject::~IDataObject\n");
	if( m_cRefCount != 0 )
		printf("Deleting %s too early 0x%x.m_cRefCount = %d\n", "IDataObject", this, m_cRefCount);
#endif //_DEBUG
	Empty();
	if(enums){
		for(UINT i = 0;i<numenums;i++)
			if(enums[i]) delete enums[i];
		free(enums);
		enums = NULL; numenums = 0;
	}
	if( pthis ) *pthis = NULL;
}

void CDataObject::DisableSetData( void ){
	allowSetData = false;
}

void CDataObject::Empty( void ){
	if(data){
		for( UINT i = 0;i<numdata;i++)
			ReleaseStgMedium(&(data[i].medium));
		free(data);
		data = NULL;
	}
}


//IUnknown members
STDMETHODIMP CDataObject::QueryInterface( REFIID iid, void** ppvObject )
{
#ifdef _DEBUG
	printf("IDataObject::QueryInterface\n");
#endif //_DEBUG

	*ppvObject = NULL;

	if ( iid == IID_IUnknown ) *ppvObject = (IUnknown*)this;
	else if ( iid == IID_IDataObject ) *ppvObject = (IDataObject*)this;

	if(*ppvObject){
		((IUnknown*)*ppvObject)->AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CDataObject::AddRef( void )
{
#ifdef _DEBUG
	printf("IDataObject::AddRef\n");
#endif //_DEBUG
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CDataObject::Release( void )
{
#ifdef _DEBUG
	printf("IDataObject::Release\n");
#endif //_DEBUG
	if( --m_cRefCount == 0 && deleteself ) delete this;
	return m_cRefCount;
}


//IDataObject members
STDMETHODIMP CDataObject::GetData( FORMATETC* pFormatetc, STGMEDIUM* pmedium )
{
#ifdef _DEBUG
	printf("IDataObject::GetData\n");
#endif //_DEBUG
	//Error handling
	//Make sure we can read from *pFormatetc
	try{ LONG i = pFormatetc->lindex; i; }
	catch(...){ return DV_E_FORMATETC; }

	//Make sure we can write to *pmedium
	try{ Zero(*pmedium); }
	catch(...){ return E_INVALIDARG; }

	if( pFormatetc->lindex != -1 ) return DV_E_LINDEX;
	if( pFormatetc->ptd != NULL ) return DV_E_FORMATETC;

	UINT formatindex = (UINT)-1;
	UINT aspectindex = (UINT)-1;
	//Search for the requested aspect of the requested format
	for( UINT i = 0; i < numdata; i++ ){
		if( data[i].format.cfFormat == pFormatetc->cfFormat ){
			formatindex = i;
			if( data[i].format.dwAspect == pFormatetc->dwAspect){
				aspectindex = i;
				break;
			}
		}
	}

	//Couldn't find the requested aspect of the requested format
	if( formatindex!=(UINT)-1 && aspectindex==(UINT)-1 ) return DV_E_DVASPECT;
	else if( formatindex==(UINT)-1 && aspectindex==(UINT)-1 ) return DV_E_FORMATETC;

	pmedium->tymed = pFormatetc->tymed;
	/*STGMEDIUM *tm = */TransferMedium( pmedium, &data[formatindex].medium );
	return GetLastError();
}

STDMETHODIMP CDataObject::GetDataHere( FORMATETC* pFormatetc, STGMEDIUM* pmedium )
{
	UNREFERENCED_PARAMETER( pFormatetc );
	UNREFERENCED_PARAMETER( pmedium );
#ifdef _DEBUG
	printf("IDataObject::GetDataHere\n");
#endif //_DEBUG
	pmedium->pUnkForRelease = NULL;
	return E_NOTIMPL;
}

STDMETHODIMP CDataObject::QueryGetData( FORMATETC* pFormatetc )
{
#ifdef _DEBUG
	printf("IDataObject::QueryGetData\n");
#endif //_DEBUG
	for( UINT i = 0; i < numdata; i++ ){
		if( data[i].format.cfFormat == pFormatetc->cfFormat ){
			if( data[i].format.dwAspect == pFormatetc->dwAspect){
				return S_OK;
			}
		}
	}
	return DV_E_FORMATETC;
}

STDMETHODIMP CDataObject::GetCanonicalFormatEtc( FORMATETC* pFormatetcIn, FORMATETC* pFormatetcOut )
{
	UNREFERENCED_PARAMETER( pFormatetcIn );
	UNREFERENCED_PARAMETER( pFormatetcOut );
#ifdef _DEBUG
	printf("IDataObject::GetCanonicalFormatEtc\n");
#endif //_DEBUG
	return E_NOTIMPL;
}

STDMETHODIMP CDataObject::SetData( FORMATETC* pFormatetc, STGMEDIUM* pmedium, BOOL fRelease )
{
#ifdef _DEBUG
	printf("IDataObject::SetData\n");
#endif //_DEBUG

	if(!allowSetData) return E_NOTIMPL;

	//Error handling
	//Make sure we can read from *pFormatetc & *pmedium & they have the same medium
	try{ if( pFormatetc->tymed != pmedium->tymed ) return DV_E_TYMED; }
	catch(...){ return E_INVALIDARG; }

	//Create a new entry
	{//<- Brace is so t goes out of scope
		void*t = realloc( data, (numdata+1)*sizeof(*data) );
		if( t == NULL ) return E_OUTOFMEMORY;
		data = (DataSpecifier*)t;
	}

	if( fRelease ){
		/*Mmmwwhhahahahahhaha we own joo
		  so there's nothing to do
		  but scratch my nuts*/
		data[numdata].medium = *pmedium;
		/*I wonder if we should be AddRef'ing IStream's & IStorage's & testing other media
		  & then if the medium is broken realloc it??*/
		/*if(pmedium){
			switch(pmedium->tymed){
				case TYMED_NULL:
				case TYMED_HGLOBAL: GMEM_INVALID_HANDLE==GlobalFlags(pmedium->hGlobal); break;
				case TYMED_FILE: IMalloc*m;CoGetMalloc(1,&m);1!=m->DidAlloc(pmedium->lpszFileName); !FileExists(pmedium->lpszFileName); break;
				case TYMED_ISTREAM: try{pmedium->pstm->AddRef();}catch(...){error} break;
				case TYMED_ISTORAGE: try{pmedium->pstg->AddRef();}catch(...){error} break;
				case TYMED_GDI: !GetObjectType((HGDIOBJ)pmedium->hBitmap); break;
				case TYMED_MFPICT: METAFILEPICT* pMFP; NULL==(pMFP=GlobalLock(pmedium->hMetaFilePict)); OBJ_METAFILE!=GetObjectType((HGDIOBJ)pMFP->hMF); GlobalUnlock(pmedium->hMetaFilePict); break;
				case TYMED_ENHMF: OBJ_ENHMETAFILE!=GetObjectType((HGDIOBJ)pmedium->hEnhMetaFile); break;
			}
		}*/
	} else {
		//Damn we need a copy
		Zero(data[numdata].medium);
		if( NULL==TransferMedium( &data[numdata].medium, pmedium ) ){
			/*SetData isn't supposed to return STG_E_MEDIUMFULL,
			but TransferMedium needs to for the sake of GetData/Here*/
			DWORD r = GetLastError();
			if( r == STG_E_MEDIUMFULL ) r = (DWORD)E_OUTOFMEMORY;
			return r;
		}
	}

	data[numdata++].format = *pFormatetc;

	return S_OK;
}

STDMETHODIMP CDataObject::EnumFormatEtc( DWORD dwDirection, IEnumFORMATETC** ppenumFormatetc )
{
#ifdef _DEBUG
	printf("IDataObject::EnumFormatEtc\n");
#endif //_DEBUG

	//Don't support DATADIR_SET since we accept any format
	if(dwDirection!=DATADIR_GET) return E_NOTIMPL;

	*ppenumFormatetc = NULL;//FIXME: should we do this?

	/*Find an array member that is NULL (has been freed)
	  or resize the array to make space for one*/
	unsigned int i = numenums;
	if(enums) for( i = 0; i < numenums && enums[i] != NULL; i++ );
	CEnumFORMATETC** t;
	if( i == numenums ) t = (CEnumFORMATETC**) realloc(enums,sizeof(CEnumFORMATETC*)*(numenums+1));
	else t = enums;
	if(t){
		enums = t; t[i] = new CEnumFORMATETC(this,true,&t[i]);
		if(t[i]){
			HRESULT ret = t[i]->QueryInterface(IID_IEnumFORMATETC,(void**)ppenumFormatetc);
			if( i == numenums ) numenums++;
			if( ret != S_OK || *ppenumFormatetc == NULL )
				//FIXME: Should we return E_INVALIDARG or E_OUTOFMEMORY or what?
				return E_OUTOFMEMORY;
		}
		else return E_OUTOFMEMORY;
	} else return E_OUTOFMEMORY;
	return S_OK;
}

//Return true if medium is valid
bool ValidateMedium(STGMEDIUM*m){
	bool ret = false;
	try{
		switch(m->tymed){
			case TYMED_HGLOBAL: if(GMEM_INVALID_HANDLE!=GlobalFlags(m->hGlobal)) ret=true; break;
			case TYMED_FILE: if(TRUE==PathFileExistsW((WCHAR*)m->lpszFileName)) ret=true; break;
			case TYMED_ISTREAM: m->pstm->AddRef(); m->pstm->Release(); ret=true; break;
			case TYMED_ISTORAGE: m->pstg->AddRef(); m->pstg->Release(); ret=true; break;
			case TYMED_GDI: if(0!=GetObjectType((HGDIOBJ)m->hBitmap)) ret=true; break;
			case TYMED_MFPICT: {METAFILEPICT*pMFP=(METAFILEPICT*)GlobalLock(m->hMetaFilePict); if(NULL!=pMFP){ if(OBJ_METAFILE==GetObjectType((HGDIOBJ)pMFP->hMF)) ret=true; GlobalUnlock(m->hMetaFilePict); }} break;
			case TYMED_ENHMF: if(OBJ_ENHMETAFILE==GetObjectType((HGDIOBJ)m->hEnhMetaFile)) ret=true; break;
			default: if(NULL==m->hGlobal) ret=true; break;
		}
	}catch(...){}
	return ret;
}

/*All the real work is done in TransferMedium
This function returns a pointer to the STGMEDIUM that contains info on the destination medium
a NULL return indicates an error & the thread's last error code is set
Error codes:
	E_INVALIDARG      either dest/src was non-NULL & reading failed or src was NULL
	E_OUTOFMEMORY     if dst was NULL couldn't CoTaskMemAlloc a new struct
	STG_E_MEDIUMFULL  the creation of the new medium failed or the existing
*/
STGMEDIUM* TransferMedium( STGMEDIUM* dest, STGMEDIUM* source, bool allowresize, bool cpysmlr ){
#define E(a) {SetLastError((DWORD)(a)); return NULL;}

	//Check if source is a valid pointer
	try{ DWORD i = source->tymed; i; }
	catch(...) E(E_INVALIDARG)


	//Check if dest is a valid pointer
	bool destallocd = false;
	try{ DWORD i = dest->tymed; i; }
	catch(...){
		//dest is non-NULL & cannot be accessed
		if( dest ){ E(E_INVALIDARG) }
		//dest is NULL, we can safely assume that we can malloc a new one
		else{
			dest = (STGMEDIUM*)CoTaskMemAlloc(sizeof(*dest));
			if( !dest ) E(STG_E_MEDIUMFULL)
			destallocd = true;
			Zero(*dest);
		}
	}

#undef E
#define E(a) { e = (DWORD)(a); goto CLEANUP; }
	DWORD e;

	//dest & source now exist as valid struct's in mem

	//Check for media defects
	{
		//Check source
		if(!ValidateMedium(source)) E(DV_E_TYMED)

		//Check dest
		//Count no. acceptable media
		BYTE n = 0;
		for(BYTE i = 0;i<32;i++){
			if( (dest->tymed >> i) & 1 ) n++;
		}

		//If there is only one format then check it
		if( n==1 ) if(!ValidateMedium(source)) E(DV_E_TYMED)
		else if( dest->hGlobal ) E(DV_E_TYMED)
	}

	STGMEDIUM dst, src; bool resize, copysmaller;
	dst = *dest; src = *source;
	resize = allowresize; copysmaller = cpysmlr;

	//Choose dst tymed
	//If TYMED_NULL or they can use same TYMED as src then use src TYMED
	if( !dst.tymed || dst.tymed & src.tymed ){
		dst.tymed = src.tymed;
	}
	/*Otherwise we need to check different TYMED's in order of preference
	  for each different TYMED*/
	else switch(src.tymed){
			//Commented out stuff may be supported sooner or later
			//eventually though, almost all transfers will be supported
#define CASE(a) else if( dst.tymed & a ){ dst.tymed = a; }
#define ENDCASE else{ dst.tymed = TYMED_NULL; }
			case TYMED_HGLOBAL: if(0);CASE(TYMED_FILE)/*CASE(TYMED_ISTREAM)CASE(TYMED_ISTORAGE)*/ENDCASE break;
			case TYMED_FILE: if(0);CASE(TYMED_HGLOBAL)/*CASE(TYMED_ISTREAM)CASE(TYMED_ISTORAGE)*/ENDCASE break;
			case TYMED_ISTREAM: if(0);/*CASE(TYMED_HGLOBAL)CASE(TYMED_FILE)CASE(TYMED_ISTORAGE)*/ENDCASE break;
			case TYMED_ISTORAGE: if(0);/*CASE(TYMED_HGLOBAL)CASE(TYMED_FILE)CASE(TYMED_ISTREAM)*/ENDCASE break;
			case TYMED_GDI: if(0);/*CASE(TYMED_MFPICT)CASE(TYMED_ENHMF)*/ENDCASE break;
			case TYMED_MFPICT: if(0);CASE(TYMED_FILE)/*CASE(TYMED_ENHMF)CASE(TYMED_GDI)*/ENDCASE break;
			case TYMED_ENHMF: if(0);CASE(TYMED_FILE)CASE(TYMED_MFPICT)/*CASE(TYMED_GDI)*/ENDCASE break;
#undef ENDCASE
#undef CASE
	}

	if( !dst.tymed ) E(DV_E_TYMED)

	if( !dst.hGlobal ) resize = true;

	//Duplicate data
	//In case of failure the caller's data should not be modified
	//Don't leak our data
	UINT size, dstsize;bool notdone, alloced;
	notdone = true; alloced = false;
	switch(src.tymed){
		case TYMED_HGLOBAL:{
			switch(dst.tymed){
				case TYMED_HGLOBAL:{
					size = GlobalSize(src.hGlobal);
					if(size){
						dstsize = GlobalSize(dst.hGlobal);
						//Ensure that dst is large enough
						if( size != dstsize ){
							if( resize ){
								dst.hGlobal = GlobalAlloc(GHND|GMEM_DDESHARE,size);
								if( !dst.hGlobal ) E(STG_E_MEDIUMFULL)
								alloced = true;
							}
							else if( dstsize < size || !copysmaller ) E(STG_E_MEDIUMFULL)
						}
						//dst should now be the right size
						void*d=GlobalLock(dst.hGlobal);//This should not fail
						if(d){
							void*s=GlobalLock(src.hGlobal);//This should not fail
							if(s){
								memcpy(d,s,size);//No failure
								notdone=false;
								GlobalUnlock(src.hGlobal);
							}
							GlobalUnlock(dst.hGlobal);
						}
						//Something strange happened
						if(notdone){
							if(alloced){ GlobalFree(dst.hGlobal); dst.hGlobal = NULL; }
							E(E_UNEXPECTED)
						}
					}
					//Someone's been buggering with the src since we checked it before
					else E(E_UNEXPECTED)
				} break;
#ifndef __CYGWIN__
				case TYMED_FILE:{
					size = GlobalSize(src.hGlobal);
					if(size){
						if(!dst.lpszFileName){
							dst.lpszFileName=(LPWSTR)CoTaskMemAlloc((MAX_PATH+1)*sizeof(WCHAR));
							if(dst.lpszFileName){
								if(!(GetTempPathW(MAX_PATH,dst.lpszFileName)
									&& GetTempFileNameW(dst.lpszFileName,L"",0,dst.lpszFileName))){
									CoTaskMemFree(dst.lpszFileName);
									dst.lpszFileName=NULL;
								}
								else alloced = true;
							}
						}
						if(dst.lpszFileName){
							int d = _wopen(dst.lpszFileName,(resize?_O_CREAT|_O_SHORT_LIVED:0)|_O_WRONLY|_O_BINARY|_O_SEQUENTIAL,_S_IREAD|_S_IWRITE);
							if(d!=-1){
								bool allocedfile = false;
								dstsize = _filelength(d);
								//Ensure that dst is large enough
								if( size != dstsize ){
									if( resize ){
										if( -1==_chsize( d, size ) ){ CoTaskMemFree(dst.lpszFileName); _close(d); E(STG_E_MEDIUMFULL) }
										allocedfile = true;
									}
									else if( dstsize < size || !copysmaller ){ CoTaskMemFree(dst.lpszFileName); _close(d); E(STG_E_MEDIUMFULL) }
								}
								//dst should now be the right size
								void*s=GlobalLock(src.hGlobal);
								if(s){
									if( -1 != _write(d,s,size) ) notdone=false;
									GlobalUnlock(src.hGlobal);
								}
								_close(d);
								if( notdone && resize ) _wremove(dst.lpszFileName);
							}
							if(notdone){
								if(alloced){
									CoTaskMemFree(dst.lpszFileName);
									dst.lpszFileName=NULL;
								}
								if(d==-1&&resize) E(STG_E_MEDIUMFULL)
								else E(E_UNEXPECTED)
							}
						} else E(STG_E_MEDIUMFULL)
					} else E(E_UNEXPECTED)
				} break;
#endif //__CYGWIN__
				/*Unsupported as yet
				case TYMED_ISTREAM:{} break;
				case TYMED_ISTORAGE:{} break;
				case TYMED_GDI:{} break;
				case TYMED_MFPICT:{} break;
				case TYMED_ENHMF:{} break;*/
						   }
			} break;
			case TYMED_FILE:{
				switch(dst.tymed){
				case TYMED_FILE:{
					if(!dst.lpszFileName){
						dst.lpszFileName=(LPWSTR)CoTaskMemAlloc((MAX_PATH+1)*sizeof(WCHAR));
						if(dst.lpszFileName) alloced = true;
					}
					if(dst.lpszFileName){
						if(!(GetTempPathW(MAX_PATH,dst.lpszFileName)
							&& GetTempFileNameW(dst.lpszFileName,L"",0,dst.lpszFileName)
							&& CopyFileW(src.lpszFileName,dst.lpszFileName,resize?FALSE:TRUE))){
							if(alloced){
								CoTaskMemFree(dst.lpszFileName);
								dst.lpszFileName=NULL;
							}
							E(STG_E_MEDIUMFULL)
						} else notdone = true;
					}
					else E(STG_E_MEDIUMFULL)
				} break;
#ifndef __CYGWIN__
				case TYMED_HGLOBAL:{
					int s = _wopen(src.lpszFileName,_O_WRONLY|_O_BINARY|_O_SEQUENTIAL, _S_IWRITE);
					if(s!=-1){
						size = _filelength(s);
						dstsize = GlobalSize(dst.hGlobal);
						//Ensure that dst is large enough
						if( size != dstsize ){
							if( resize ){
								dst.hGlobal = GlobalAlloc(GHND|GMEM_DDESHARE,size);
								if( !dst.hGlobal ) E(STG_E_MEDIUMFULL)
									alloced = true;
							}
							else if( dstsize < size || !copysmaller ) E(STG_E_MEDIUMFULL)
						}
						//dst should now be the right size
						void*d=GlobalLock(dst.hGlobal);
						if(s){
							if( -1 != _read(s,d,size) ) notdone=false;
							GlobalUnlock(dst.hGlobal);
						}
						_close(s);
					}
				} break;
#endif //__CYGWIN__
				/*Unsupported as yet
				case TYMED_ISTREAM:{} break;
				case TYMED_ISTORAGE:{} break;
				case TYMED_GDI:{} break;
				case TYMED_MFPICT:{} break;
				case TYMED_ENHMF:{} break;*/
			}
		} break;
		case TYMED_ISTREAM:{
			switch(dst.tymed){
				case TYMED_ISTREAM:{
					if(dst.pstm||S_OK==/*??*/CreateStreamOnHGlobal(NULL,TRUE,&(dst.pstm)) && !!(alloced=true)/*How do i disable assignment in conditional warning?*/ ){
						if(alloced){
							LARGE_INTEGER zero={0};
							src.pstm->Seek(zero,STREAM_SEEK_SET,NULL);
							dst.pstm->Seek(zero,STREAM_SEEK_SET,NULL);
						}
						ULARGE_INTEGER max; max.LowPart = max.HighPart = 0xffffffff;
						if(S_OK!=src.pstm->CopyTo(dst.pstm,max,NULL,NULL)){
							dst.pstm->Release();
						}
					}
				} break;
				/*Unsupported as yet
				case TYMED_HGLOBAL:{} break;
				case TYMED_FILE:{} break;
				case TYMED_ISTORAGE:{} break;
				case TYMED_GDI:{} break;
				case TYMED_MFPICT:{} break;
				case TYMED_ENHMF:{} break;*/
			}
		} break;
		case TYMED_ISTORAGE:{
			switch(dst.tymed){
				case TYMED_ISTORAGE:{
					if((dst.pstg||S_OK==/*??*/StgCreateDocfile(NULL,STGM_READWRITE|STGM_SHARE_DENY_NONE|STGM_DELETEONRELEASE|STGM_CREATE,0,&(dst.pstg)))
					 &&S_OK!=src.pstg->CopyTo(0,NULL,NULL,dst.pstg)){
						dst.pstg->Release();
					}
				} break;
				/*Unsupported as yet
				case TYMED_HGLOBAL:{} break;
				case TYMED_FILE:{} break;
				case TYMED_ISTREAM:{} break;
				case TYMED_GDI:{} break;
				case TYMED_MFPICT:{} break;
				case TYMED_ENHMF:{} break;*/
			}
		} break;
		case TYMED_GDI:{
			switch(dst.tymed){
				case TYMED_GDI:{
					 E(DV_E_TYMED)
					/* How should this be done ?
					OleDuplicateData((HANDLE)src.hBitmap,CF_BITMAP,GHND|GMEM_DDESHARE);
					switch(GetObjectType((HGDIOBJ)src.hBitmap)){
						case OBJ_BITMAP: CopyBitmap(); break;
						case OBJ_BRUSH: break;
						case OBJ_FONT: break;
						case OBJ_PAL: break;
						case OBJ_PEN: break;
						case OBJ_EXTPEN: break;
						case OBJ_REGION: break;
						case OBJ_DC: break;
						case OBJ_MEMDC: break;
						case OBJ_METAFILE: break;
						case OBJ_METADC: break;
						case OBJ_ENHMETAFILE: break;
						case OBJ_ENHMETADC : break;
						default: break;
					}*/
				} break;
				/*Unsupported as yet
				case TYMED_HGLOBAL:{} break;
				case TYMED_FILE:{} break;
				case TYMED_ISTREAM:{} break;
				case TYMED_ISTORAGE:{} break;
				case TYMED_MFPICT:{} break;
				case TYMED_ENHMF:{} break;*/
			}
		} break;
		case TYMED_MFPICT:{
			switch(dst.tymed){
				case TYMED_MFPICT:{
					size=GlobalSize(src.hMetaFilePict);
					if( size ){
						if(dst.hMetaFilePict){
							if(!resize) E(DV_E_TYMED)
						}
						else dst.hMetaFilePict=(METAFILEPICT*)GlobalAlloc(GHND|GMEM_DDESHARE,size);
						if(dst.hMetaFilePict){
							try{
								METAFILEPICT*d=(METAFILEPICT*)GlobalLock(dst.hMetaFilePict);
								if(d){
									METAFILEPICT*s=(METAFILEPICT*)GlobalLock(src.hMetaFilePict);
									if(s){
										if(NULL!=(d->hMF=CopyMetaFile(s->hMF,NULL))){
											notdone=FALSE;
										}
										GlobalUnlock(src.hMetaFilePict);
									}
									GlobalUnlock(dst.hMetaFilePict);
								}
							} catch(...){}
							if(notdone){
								GlobalFree(dst.hMetaFilePict);
								dst.hMetaFilePict=NULL;

							}
						}
					}
				} break;
				case TYMED_FILE:{
					if(!dst.lpszFileName){
						dst.lpszFileName=(LPWSTR)CoTaskMemAlloc((MAX_PATH+1)*sizeof(WCHAR));
						if(dst.lpszFileName){
							if(!(GetTempPathW(MAX_PATH,dst.lpszFileName)
								&& GetTempFileNameW(dst.lpszFileName,L"",0,dst.lpszFileName))){
								CoTaskMemFree(dst.lpszFileName);//Don't leak
								dst.lpszFileName=NULL;
							}
							else alloced = true;
						} else E(STG_E_MEDIUMFULL)
					}
					HMETAFILE hemf = 0;
					try{
						METAFILEPICT*s=(METAFILEPICT*)GlobalLock(src.hMetaFilePict);
						if(s){
							hemf = CopyMetaFileW(s->hMF,dst.lpszFileName);
							GlobalUnlock(src.hMetaFilePict);
						}
					} catch(...){}
					if(hemf) DeleteMetaFile(hemf);
					else{
						if(alloced){
							CoTaskMemFree(dst.lpszFileName);
							dst.lpszFileName=NULL;
						}
						E(STG_E_MEDIUMFULL)
					}
				} break;
				/*Unsupported as yet
				case TYMED_HGLOBAL:{} break;
				case TYMED_ISTREAM:{} break;
				case TYMED_ISTORAGE:{} break;
				case TYMED_GDI:{} break;
				case TYMED_ENHMF:{} break;*/
			}
		} break;
		case TYMED_ENHMF:{
			switch(dst.tymed){
				case TYMED_ENHMF:{
					if(!dst.hEnhMetaFile) dst.hEnhMetaFile = CopyEnhMetaFile(src.hEnhMetaFile,NULL);
					/*else if(!resize&&size==dstsize){ This bit is unsupported as yet dstsize = GetEnhMetaFileBits(src.hEnhMetaFile, 0, NULL); GetEnhMetaFileBits(src.hEnhMetaFile, size, buffer); SetEnhMetaFileBits(size, buffer) }*/
					/*else if(resize){ This bit is unsupported as yet }*/
					else E(STG_E_MEDIUMFULL)
				} break;
				case TYMED_FILE:{
					if(!dst.lpszFileName){
						dst.lpszFileName=(LPWSTR)CoTaskMemAlloc((MAX_PATH+1)*sizeof(WCHAR));
						if(dst.lpszFileName){
							if(!(GetTempPathW(MAX_PATH,dst.lpszFileName)
								&& GetTempFileNameW(dst.lpszFileName,L"",0,dst.lpszFileName))){
								CoTaskMemFree(dst.lpszFileName);//Don't leak
								dst.lpszFileName=NULL;
							}
							else alloced = true;
						} else E(STG_E_MEDIUMFULL)
					}
					HENHMETAFILE hemf = CopyEnhMetaFileW(src.hEnhMetaFile,dst.lpszFileName);
					if(hemf) DeleteEnhMetaFile(hemf);
					else{
						if(alloced){
							CoTaskMemFree(dst.lpszFileName);
							dst.lpszFileName=NULL;
						}
						E(STG_E_MEDIUMFULL)
					}
				} break;
				case TYMED_MFPICT:{
					if(dst.hMetaFilePict) E(E_INVALIDARG)
					HMETAFILE hmf = NULL;
					HDC hrefDC = GetDC(NULL);
					if ( hrefDC ){
						//Be sure to use the same MM_xx as below
						size = GetWinMetaFileBits(src.hEnhMetaFile, 0, NULL, MM_ANISOTROPIC, hrefDC);
						if ( size ){
							//Allocate enough memory to hold metafile bits
							//Pity there is no way to transfer them directly. damn Win32
							BYTE* buffer = new BYTE[size];
							//Get the bits of the enhanced metafile
							if (buffer){
								UINT got = GetWinMetaFileBits(src.hEnhMetaFile, size,(LPBYTE)buffer, MM_ANISOTROPIC, hrefDC);
								if(got>=size){
									METAFILEPICT*d=(METAFILEPICT*)GlobalLock(dst.hMetaFilePict);
									if(d){
										//What the f**k should I do here?
										d->mm = MM_ANISOTROPIC; d->xExt = d->yExt = 0;
										//Copy the bits into a memory based Windows metafile
										d->hMF = hmf = SetMetaFileBitsEx(got, buffer);
										GlobalUnlock(dst.hMetaFilePict);
									}
								}
								//Done with the actual memory used to store bits so nuke it
								delete[]buffer;
							}
						}
						ReleaseDC( NULL, hrefDC );
					}
					if(!hmf){ E(STG_E_MEDIUMFULL) }
				} break;
				/*Unsupported as yet
				case TYMED_GDI:{} break;
				case TYMED_HGLOBAL:{} break;
				case TYMED_ISTREAM:{} break;
				case TYMED_ISTORAGE:{} break;*/
			}
		} break;
	}

	*dest = dst;

	E( S_OK )
#undef E

CLEANUP:
	if( destallocd ) CoTaskMemFree( &dst );
	SetLastError(e);
	return NULL;
}


//CEnumFORMATETC
//Members
CEnumFORMATETC::CEnumFORMATETC( CDataObject*par, bool delself, CEnumFORMATETC** p )
{
#ifdef _DEBUG
	printf("IEnumFORMATETC::IEnumFORMATETC\n");
#endif //_DEBUG
	m_cRefCount = 0;
	deleteself = delself;
	pthis = p;

	parent = par;
	index = 0;
}

CEnumFORMATETC::~CEnumFORMATETC( void )
{
#ifdef _DEBUG
	printf("IEnumFORMATETC::~IEnumFORMATETC\n");
	if( m_cRefCount != 0 )
		printf("Deleting %s too early 0x%x.m_cRefCount = %d\n", "IEnumFORMATETC", this, m_cRefCount);
#endif //_DEBUG
	if( pthis ) *pthis = NULL;
}

//IUnknown members
STDMETHODIMP CEnumFORMATETC::QueryInterface( REFIID iid, void** ppvObject )
{
#ifdef _DEBUG
	printf("IEnumFORMATETC::QueryInterface\n");
#endif //_DEBUG

	*ppvObject = NULL;

	if ( iid == IID_IUnknown ) *ppvObject = (IUnknown*)this;
	else if ( iid == IID_IEnumFORMATETC ) *ppvObject = (IEnumFORMATETC*)this;

	if(*ppvObject){
		((IUnknown*)*ppvObject)->AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEnumFORMATETC::AddRef( void )
{
#ifdef _DEBUG
	printf("IEnumFORMATETC::AddRef\n");
#endif //_DEBUG
	return ++m_cRefCount;
}

STDMETHODIMP_(ULONG) CEnumFORMATETC::Release( void )
{
#ifdef _DEBUG
	printf("IEnumFORMATETC::Release\n");
#endif //_DEBUG
	if( --m_cRefCount == 0 && deleteself ) delete this;
	return m_cRefCount;
}

//IEnumFORMATETC members
STDMETHODIMP CEnumFORMATETC::Next( ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched ){
#ifdef _DEBUG
	printf("IEnumFORMATETC::Next\n");
#endif //_DEBUG
	ULONG fetched = 0;
	unsigned int e=index+celt;
	while( index < e && index < parent->numdata ){
		try{ rgelt[fetched] = parent->data[index].format; }
		catch(...){ break; } fetched++;index++;
	}
	try{ if( pceltFetched ) *pceltFetched = fetched; } catch(...){}
	if(fetched==celt) return S_OK;
	else return S_FALSE;
}

STDMETHODIMP CEnumFORMATETC::Skip( ULONG celt ){
#ifdef _DEBUG
	printf("IEnumFORMATETC::Skip\n");
#endif //_DEBUG
	index += celt; return S_FALSE;
}

STDMETHODIMP CEnumFORMATETC::Reset( void ){
#ifdef _DEBUG
	printf("IEnumFORMATETC::Reset\n");
#endif //_DEBUG
	index = 0; return S_OK;
}

STDMETHODIMP CEnumFORMATETC::Clone( IEnumFORMATETC** ppenum ){
#ifdef _DEBUG
	printf("IEnumFORMATETC::Clone\n");
#endif //_DEBUG
	return parent->EnumFormatEtc( DATADIR_GET, ppenum );
}


//Following methods not implemented yet
STDMETHODIMP CDataObject::DAdvise( FORMATETC* pFormatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection )
{
	UNREFERENCED_PARAMETER( pFormatetc );
	UNREFERENCED_PARAMETER( advf );
	UNREFERENCED_PARAMETER( pAdvSink );
	UNREFERENCED_PARAMETER( pdwConnection );
#ifdef _DEBUG
	printf("IDataObject::DAdvise\n");
#endif //_DEBUG
	return E_NOTIMPL;
}

STDMETHODIMP CDataObject::DUnadvise( DWORD dwConnection )
{
	UNREFERENCED_PARAMETER( dwConnection );
#ifdef _DEBUG
	printf("IDataObject::DUnadvise\n");
#endif //_DEBUG
	return E_NOTIMPL;
}

STDMETHODIMP CDataObject::EnumDAdvise( IEnumSTATDATA** ppenumAdvise )
{
	UNREFERENCED_PARAMETER( ppenumAdvise );
#ifdef _DEBUG
	printf("IDataObject::EnumDAdvise\n");
#endif //_DEBUG
	return E_NOTIMPL;
}
