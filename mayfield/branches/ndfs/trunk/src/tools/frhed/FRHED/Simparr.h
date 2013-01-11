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
	void SetAtGrow(int nIndex, T argT);
	int SetAtGrowRef(int nIndex, T& argT);
	void SetAt(int nIndex, T argT);
	void SetAtRef(int nIndex, T& argT);
	T GetAt(int nIndex);
	T& GetRefAt(int nIndex);
	int GetSize();
	int GetUpperBound();
	int GetLength();
	int GetGrowBy();
	int SetSize(int nNewSize, int nGrowBy = 0);
	void SetGrowBy(int nGrowBy);
	T& operator[](int nIndex) {return m_pT[nIndex];}
	SimpleArray<T>& operator=(SimpleArray<T>& spa);
	void ClearAll();
	int blContainsRef(T& argT);
	int blContains(T argT);
	int nContainsAt(T argT);
	int blIsEmpty();
	void AppendRef(T& argT);
	void Append(T argT);
	void Exchange(int nIndex1, int nIndex2);
	int blCompare(SimpleArray<T>& spa);
	int operator==(SimpleArray<T>& spa);
	int operator!=(SimpleArray<T>& spa);
	int Adopt(T* ptArray, int upbound, int size);
	void SetUpperBound(int upbnd);
	int AppendArray( T* pSrc, int srclen );
	int ExpandToSize();
	int CopyFrom( int index, T* pSrc, int srclen );
	int Replace( int ToReplaceIndex, int ToReplaceLength, T* pReplaceWith, int ReplaceWithLength );

protected:
	int AddSpace (int nExtend);
	T* m_pT;
	int m_nSize;
	int m_nUpperBound;
	int m_nGrowBy;
};

// A string class.
class SimpleString : public SimpleArray<char>
{
public:
	int IsEmpty();
	SimpleString operator+( SimpleString& str1 );
	SimpleString();
	SimpleString( char* ps );
	int AppendString( char* ps );
	int SetToString( char* ps );
	char* operator=( char* ps );
	SimpleString& operator=( SimpleString str );
	char* operator+=( char* ps );
	int StrLen();
	void Clear();
};

SimpleString operator+( SimpleString ps1, char* ps2 );
SimpleString operator+( char* ps1, SimpleString ps2 );

#endif // simplearr_h
