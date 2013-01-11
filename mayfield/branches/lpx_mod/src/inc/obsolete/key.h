/*
	Serial Number Encryption Key.
*/

#ifndef _KEY_H_
#define _KEY_H_

#define	NDKEY10		"45"
#define	NDKEY11		"32"
#define	NDKEY12		"56"
#define	NDKEY13		"2F"
#define	NDKEY14		"EC"
#define	NDKEY15		"4A"
#define	NDKEY16		"38"
#define	NDKEY17		"53"

#define	NDKEY20		"1E"
#define	NDKEY21		"4E"
#define	NDKEY22		"0F"
#define	NDKEY23		"EB"
#define	NDKEY24		"33"
#define	NDKEY25		"27"
#define	NDKEY26		"50"
#define	NDKEY27		"C1"

#define	NDVID		"01"

#define	NDRESERVED0	"FF"
#define	NDRESERVED1	"FF"

#define	NDRANDOM	"CD"




/*
	Serial Number Encryption Key to support old routine.
	Take this out in the future
*/
unsigned char Real2LanDiskKey[8] = { 45, 32, 56, 23, 45, 78, 78, 102 };
unsigned char LanDisk2UserKey[8] = { 34, 243, 76, 129, 1, 35, 56, 72 };



#endif	// _KEY_H_