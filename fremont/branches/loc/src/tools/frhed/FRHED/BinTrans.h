#ifndef BinTrans_h
#define BinTrans_h

#include "simparr.h"

//-------------------------------------------------------------------
class Text2BinTranslator : public SimpleString
{
public:
	int bCompareBin( Text2BinTranslator& tr2, int charmode, int binmode );
	Text2BinTranslator();
	Text2BinTranslator( char* ps );
	int GetTrans2Bin( SimpleArray<char>& sa, int charmode, int binmode );
	char* operator=( char* ps )
	{
		return SimpleString::operator=( ps );
	}

private:
	int iCreateBcTranslation( char* dest, char* src, int srclen, int charmode, int binmode );
	int iTranslateOneBytecode( char* dest, char* src, int srclen, int binmode );
	int iLengthOfTransToBin( char* src, int srclen );
	int iIsBytecode( char* src, int len );
	int translate_bytes_to_BC (char* pd, unsigned char* src, int srclen);
	int iBytes2BytecodeDestLen( char* src, int srclen );
	int iFindBytePos( char* src, char c );
	char cTranslateAnsiToOem( char c );
};

#endif // BinTrans_h
