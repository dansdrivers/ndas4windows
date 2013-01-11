#pragma once
#include <windows.h>

namespace ximeta {

	template <typename T, size_t MaxBuffer = 32>
	class CStringizer
	{
	public:

		CStringizer() : m_cchStrBuf(MaxBuffer)
		{
		}

	protected:

		const size_t m_cchStrBuf;
		TCHAR m_lpStrBuf[MaxBuffer];

	};

}
