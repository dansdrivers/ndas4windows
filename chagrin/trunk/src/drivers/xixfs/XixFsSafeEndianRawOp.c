#include "XixFsType.h"
#include "XixFsDebug.h"
#include "XixFsDiskForm.h"


/*
	For Addr information of File function
 */
VOID
ChangeAddrBtoL(
	IN uint8 * Addr,
	IN uint32  BlockSize
)
{
	uint64 * tmpAddr = NULL;
	uint32 i = 0;
	uint32 maxLoop = 0;

	tmpAddr = (uint64 *)Addr;
	maxLoop = (uint32)( BlockSize/sizeof(uint64));	
	
	for(i = 0; i < maxLoop; i++){
		XixChang64BigToLittle(&tmpAddr[i]);
	}

	return;
}

VOID
ChangeAddrLtoB(
	IN uint8 * Addr,
	IN uint32  BlockSize
)
{
	uint64 * tmpAddr = NULL;
	uint32 i = 0;
	uint32 maxLoop = 0;

	tmpAddr = (uint64 *)Addr;
	maxLoop = (uint32)( BlockSize/sizeof(uint64));	
	
	for(i = 0; i < maxLoop; i++){
		XixChange64LittleToBig(&tmpAddr[i]);
	}

	return;
}


NTSTATUS
XifsdEndianSafeReadAddrInfo
(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;

	try{
		RC = XifsdRawReadBlockDevice(
			DeviceObject,
			Offset,
			BlockSize, 
			Addr
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XifsdRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
		if(NT_SUCCESS(RC)){
			ChangeAddrLtoB(Buffer,Length);
		}
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}




NTSTATUS
XifsdEndianSafeWriteAddrInfo
(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	OUT uint8			*Buffer
)
{
	NTSTATUS RC = STATUS_SUCCESS;

#if (_M_PPC || _M_MPPC) 
	ChangeAddrBtoL(Buffer,Length);
#endif //#if (_M_PPC || _M_MPPC) 		

	try{
		RC = XifsdRawWriteBlockDevice(
			DeviceObject,
			Offset,
			BlockSize, 
			Addr
			);	
		
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error ReadAddLot:XifsdRawReadBlockDevice Status(0x%x)\n", RC));

			RC = STATUS_UNSUCCESSFUL;
			try_return(RC);
		}		
	}finally{
#if (_M_PPC || _M_MPPC) 
			ChangeAddrLtoB(Buffer,Length);
#endif //#if (_M_PPC || _M_MPPC) 		
	}

	return RC;
}