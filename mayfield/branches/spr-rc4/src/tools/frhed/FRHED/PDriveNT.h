//=========================================================
// File: simparr.h

#ifndef simplearr_h
#define simplearr_h

#define ARR_EMPTY -1

#include <string.h>

template<class T> class SimpleArray
{
public:
	SimpleArray();
	SimpleArray(T* ptArray, int upbound, int size);
	SimpleArray(int nNewSize, int nGrowBy = 1);
	virtual ~SimpleArray();
	SimpleArray(SimpleArray& spaArg);
	operator T*()
	{
		return m_pT;
	}
	int InsertAt(int nIndex, T argT, int nCount = 1);
	void InsertAtRef(int nIndex, T& argT, int nCount = 1);
	int InsertAtGrow(int nIndex, T argT, int nCount = 1);
	void InsertAtGrowRef(int nIndex, T& argT, int nCount = 1);
	int InsertAtGrow (int nIndex, T* pT, int nSrcIndex, int nCount);
	int RemoveAt(int nIndex, int nCount = 1);
	void SetAtGrow(int nIndex, T a