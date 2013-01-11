#include "XixFsDiskForm.h"


VOID
XixChange16LittleToBig(
	IN uint16	* Addr
)
{
	*Addr = LITTLETOBIGS(*Addr);
}


VOID
XixChange16BigToLittle(
	IN uint16	* Addr
)
{
	*Addr = BIGTOLITTLES(*Addr);
}


VOID
XixChange32LittleToBig(
	IN uint32	* Addr
)
{
	*Addr = LITTLETOBIGL(*Addr);
}


VOID
XixChange32BigToLittle(
	IN uint32	* Addr
)
{
	*Addr = BIGTOLITTLEL(*Addr);
}




VOID
XixChange64LittleToBig(
	IN uint64	* Addr
)
{
	*Addr = LITTLETOBIGLL(*Addr);
}


VOID
XixChange64BigToLittle(
	IN uint64	* Addr
)
{
	*Addr = BIGTOLITTLELL(*Addr);
}








