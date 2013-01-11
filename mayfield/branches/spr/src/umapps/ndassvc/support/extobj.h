#pragma once

namespace ximeta {

	struct IExtensibleObject
	{
		virtual ULONG AddRef(void) = 0;
		virtual ULONG Release(void) = 0;
	};

	class CExtensibleObject : public IExtensibleObject
	{
		LONG m_cRef;
	protected:
		CExtensibleObject() : m_cRef(0) {}

	public:
		virtual ~CExtensibleObject() {}

		virtual ULONG AddRef(void) 
		{ 
			return (ULONG) ::InterlockedIncrement(&m_cRef); 
		}

		virtual ULONG Release(void) 
		{
			_ASSERTE(m_cRef > 0);
			if (m_cRef == 0) {
				OutputDebugString(_T("Reference Error: Release called at m_cRef = 0!!!\n"));
			}
			ULONG ulc = ::InterlockedDecrement(&m_cRef);
			if (0 == m_cRef) {
				delete this;
				// T* pThis = reinterpret_cast<T*>(this);
				// delete pThis;
			}
			return ulc;
		}
	};

} // namespace ximeta
