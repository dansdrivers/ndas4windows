#include "MultilangRes.h"
#include "resource.h"

int GetLanguageResourceOffset(WORD wLangId)
{
	int offset;
	switch (wLangId)
	{
	case 1031: offset = IDS_1031_OFFSET; break; // German (1031)
	case 1034: offset = IDS_1034_OFFSET; break; // Spanish (1033)
	case 1036: offset = IDS_1036_OFFSET; break; // German (1036)
	case 1040: offset = IDS_1040_OFFSET; break; // Italian (1040)
	case 1041: offset = IDS_1041_OFFSET; break; // Japanese (1041)
	case 1042: offset = IDS_1042_OFFSET; break; // Korean (1042)
	case 1046: offset = IDS_1046_OFFSET; break; // Portuguese (Brazil) (1046)
	case 1033: // English (1033)
	default:
		offset = 0;
	}
	return offset;
}

