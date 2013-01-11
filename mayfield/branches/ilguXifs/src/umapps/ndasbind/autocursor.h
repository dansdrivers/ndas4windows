#pragma once

class AutoCursor
{
protected:
	BOOL m_bReleased;
	HCURSOR m_hCursorPrev;

public:
	AutoCursor(IN LPCTSTR lpCursorName)
	{
		m_bReleased = TRUE;

		HCURSOR hCursor;
		hCursor = LoadCursor(NULL, lpCursorName);

		if(hCursor)
		{
			m_hCursorPrev = SetCursor(hCursor);
			m_bReleased = FALSE;
		}
	}

	~AutoCursor()
	{
		Release();
	}

	void Release(BOOL bRestoreCursor = TRUE)
	{
		if(bRestoreCursor && !m_bReleased)
		{
			SetCursor(m_hCursorPrev);
		}

		m_bReleased = TRUE;
	}
};

