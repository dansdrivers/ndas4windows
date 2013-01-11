#pragma once

template <typename T, bool managed = true>
class CBuffer
{
public:
	LPVOID m_lpBuffer;

	CBuffer() : m_lpBuffer(NULL) {}

	~CBuffer() 
	{ 
		if (managed) 
		{
			T* pT = reinterpret_cast<T*>(this); 
			pT->Free();
		}
	}

	LPVOID Alloc(DWORD dwBytes, DWORD dwFlags = 0)
	{
		if (managed) {
			Free();
		}
		T* pT = reinterpret_cast<T*>(this); 
		return pT->Alloc(dwBytes, dwFlags);
	};

	void Free() 
	{
		if (m_lpBuffer) {
			T* pT = reinterpret_cast<T*>(this); 
			return pT->Free();
		}
	};
};

class CHeapBuffer : public CBuffer<CHeapBuffer>
{
public:
	CHeapBuffer()
	{
	}

	~CHeapBuffer()
	{
		Free();
	}

	LPVOID Alloc(DWORD dwBytes, DWORD dwFlags = 0)
	{
		return m_lpBuffer = ::HeapAlloc(::GetProcessHeap(), dwFlags, dwBytes);
	}

	void Free()
	{
		if (m_lpBuffer) {
			::HeapFree(::GetProcessHeap(), 0, m_lpBuffer);
			m_lpBuffer = NULL;
		}
	}

};
