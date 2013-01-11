#ifndef idt_h
#define idt_h


interface CDropTarget : public IDropTarget
{
	private:
		ULONG m_cRefCount;
		bool deleteself;
		CDropTarget** pthis;

		DWORD LastKeyState;

		bool hdrop_present;
		IDataObject* pDataObj;

	public:
		//Members
		CDropTarget( bool delself = false, CDropTarget** p = NULL );
		~CDropTarget( void );

		//IUnknown members
		STDMETHODIMP QueryInterface( REFIID iid, void** ppvObject );
		STDMETHODIMP_(ULONG) AddRef( void );
		STDMETHODIMP_(ULONG) Release( void );

		//IDropTarget methods
		STDMETHODIMP DragEnter( IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect );
		STDMETHODIMP DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect );
		STDMETHODIMP DragLeave( void );
		STDMETHODIMP Drop( IDataObject* pDataObject, DWORD grfKeyState, POINTL pt, DWORD * pdwEffect );
};


struct DROPPARAMS{
	DWORD allowable_effects;
	bool effect;
	UINT numformatetcs;
	FORMATETC* formatetcs;
	UINT numformats;
	UINT* formats;
};


#endif // idt_h