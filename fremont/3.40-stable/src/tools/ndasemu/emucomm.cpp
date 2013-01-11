/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"

#if 0
//////////////////////////////////////////////////////////////////////////
//
//	LPX
//

// Setting packet drop rate
BOOL
LpxSetDropRate(
	ULONG DropRate
){
	HANDLE deviceHandle;
	ULONG Param;
	DWORD dwReturn;
	BOOL bRet;
	deviceHandle = CreateFile (
	            TEXT("\\\\.\\SocketLpx"),
	            GENERIC_READ,
	            0,
	            NULL,
	            OPEN_EXISTING,
	            FILE_FLAG_OVERLAPPED,
	            0
	     );

	if( INVALID_HANDLE_VALUE == deviceHandle ) {
		fprintf(stderr, "CreateFile Error\n");
	} else {
		Param = DropRate;
		bRet = DeviceIoControl(
                 deviceHandle,
                 IOCTL_LPX_SET_DROP_RATE,
                 &Param,   
                 sizeof(Param), 
                 NULL,
                 NULL,
                 &dwReturn,	
                 NULL        // Overlapped
                 );	
		if (bRet == FALSE) {
			fprintf(stderr, "Failed to set drop rate\n");
		} else {
			fprintf(stderr, "Set drop rate to %d\n", Param);
		}
		CloseHandle(deviceHandle);
	}

	return bRet;
}


#endif

//////////////////////////////////////////////////////////////////////////
//
//	LanScsi Protocol
//

__inline
int 
RecvIt(
	   SOCKET	sock,
	   PUCHAR	buf, 
	   int		size
){
	int res;
	int len = size;

	while (len > 0) {
		if ((res = recv(sock, (char *)buf, len, 0)) == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "RecvIt");
			return res;
		} else if(res == 0) {
			fprintf(stderr, "RecvIt: Disconnected...\n");
			return res;
		}
		len -= res;
		buf += res;
	}

	return size;
}

__inline
int 
SendIt(
	   SOCKET	sock,
	   PUCHAR	buf, 
	   int		size
){
	int res;
	int len = size;

	while (len > 0) {
		if ((res = send(sock, (char *)buf, len, 0)) == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "SendIt");
			return res;
		} else if(res == 0) {
			fprintf(stderr, "SendIt: Disconnected...\n");
			return res;
		}
		len -= res;
		buf += res;
	}

	return size;
}


//
//	Receive a PDU and update pAHS, pHeaderDig, pDataSeg, and pDataDig 
//		of PDU PLANSCSI_PDU_POINTERS.
//

int
ReceivePdu(
	IN SOCKET					connSock,
	IN PENCRYPTION_INFO			EncryptInfo,
	IN PNDASDIGEST_INFO			DigestInfo,
	IN PLANSCSI_PDU_POINTERS	pPdu
){
	int		iResult;
	int		iTotalRecved = 0;
	PUCHAR	pPtr = pPdu->pBufferBase;

	UNREFERENCED_PARAMETER(DigestInfo);

	// Read Header.
	iResult = RecvIt(
				connSock,
				pPtr,
				sizeof(LANSCSI_H2R_PDU_HEADER));
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "ReceivePdu: Can't Recv Header...\n");

		return iResult;
	} else if(iResult == 0) {
		fprintf(stderr, "ReceivePdu: Disconnected...\n");

		return iResult;
	} else
		iTotalRecved += iResult;

//	pPdu->pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

	pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

	if(EncryptInfo && EncryptInfo->HeaderEncryptAlgo != 0) {

		Decrypt32(
			(unsigned char*)pPdu->pH2RHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&EncryptInfo->CHAP_C,
			(unsigned char *)&EncryptInfo->Password64
			);
	}

	// Read AHS.
	if(ntohs(pPdu->pH2RHeader->AHSLen) > 0) {
		iResult = RecvIt(
			connSock,
			pPtr,
			ntohs(pPdu->pH2RHeader->AHSLen)
			);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReceivePdu: Can't Recv AHS...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReceivePdu: Disconnected...\n");

			return iResult;
		} else
			iTotalRecved += iResult;
	
		pPdu->pAHS = (PCHAR)pPtr;

		pPtr += ntohs(pPdu->pH2RHeader->AHSLen);

		if ( EncryptInfo && EncryptInfo->HeaderEncryptAlgo != 0) {

			Decrypt32(
				(unsigned char*)pPdu->pAHS,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(unsigned char *)&EncryptInfo->CHAP_C,
				(unsigned char *)&EncryptInfo->Password64);
		}
	}

	// Read Header Dig.
	pPdu->pHeaderDig = NULL;

	// Read Data segment.
	if(ntohl(pPdu->pH2RHeader->DataSegLen) > 0) {
		iResult = RecvIt(
			connSock,
			pPtr,
			ntohl(pPdu->pH2RHeader->DataSegLen)
			);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReceivePdu: Can't Recv Data segment...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReceivePdu: Disconnected...\n");

			return iResult;
		} else 
			iTotalRecved += iResult;
		
		pPdu->pDataSeg = (PCHAR)pPtr;
		
		pPtr += ntohl(pPdu->pH2RHeader->DataSegLen);

		
		if( EncryptInfo && EncryptInfo->BodyEncryptAlgo != 0) {

			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohl(pPdu->pH2RHeader->DataSegLen),
				(unsigned char *)&EncryptInfo->CHAP_C,
				(unsigned char *)&EncryptInfo->Password64
				);
		}

	}
	
	// Read Data Dig.
	pPdu->pDataDig = NULL;
	
	return iTotalRecved;
}

int
ReceiveBody(
	IN SOCKET			connSock,
	IN PENCRYPTION_INFO	EncryptInfo,
	IN PNDASDIGEST_INFO	DigestInfo,
	IN ULONG			DataTransferLength,
	IN ULONG			DataBufferLength,
	OUT PUCHAR			DataBuffer
){
	int		iResult;
	int		iTotalRecved = 0;

	UNREFERENCED_PARAMETER(DigestInfo);

	//
	//	Parameter check
	//

	if(DataBuffer == NULL)
		return -1;
	if(DataTransferLength == 0)
		return 0;
	if(DataBufferLength < DataTransferLength)
		return -1;

	// Read Data
	iResult = RecvIt(
				connSock,
				DataBuffer,
				DataTransferLength);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "ReceiveBody: Can't Recv Body\n");

		return iResult;
	} else if(iResult == 0) {
		fprintf(stderr, "ReceiveBody: Disconnected...\n");

		return iResult;
	} else
		iTotalRecved += iResult;

	if(EncryptInfo && EncryptInfo->BodyEncryptAlgo != 0) {

		Decrypt32(
			DataBuffer,
			DataTransferLength,
			(unsigned char *)&EncryptInfo->CHAP_C,
			(unsigned char *)&EncryptInfo->Password64
			);
	}

	return iTotalRecved;
}

int
SendPdu(
	IN SOCKET					connSock,
	IN PENCRYPTION_INFO			EncryptInfo,
	IN PNDASDIGEST_INFO			DigestInfo,
	OUT PLANSCSI_PDU_POINTERS	pPdu
){

	PLANSCSI_H2R_PDU_HEADER pHeader;
	int		iAHSegLen, iDataSegLen;
	int		iResult;
	int		iTotalRecved = 0;
	PUCHAR	pPtr = pPdu->pBufferBase;

	pHeader = pPdu->pH2RHeader;
	iAHSegLen = ntohs(pHeader->AHSLen);
	iDataSegLen = ntohl(pHeader->DataSegLen);

	//
	// Encrypt Header.
	//	Assume the buffer contains header + AHS consecutively.
	//
	if(EncryptInfo && EncryptInfo->HeaderEncryptAlgo != 0) {

			Encrypt32(
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iAHSegLen,
				(unsigned char *)&EncryptInfo->CHAP_C,
				(unsigned char*)&EncryptInfo->Password64
				);
	}

	//
	// Encrypt Data.
	//
	if(EncryptInfo && EncryptInfo->BodyEncryptAlgo != 0	&& iDataSegLen > 0) {

			Encrypt32(
				(unsigned char*)pPdu->pDataSeg,
				iDataSegLen,
				(unsigned char *)&EncryptInfo->CHAP_C,
				(unsigned char*)&EncryptInfo->Password64
				);
	}

	// Send Request.
	iResult = SendIt(
				connSock,
				(PUCHAR)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iAHSegLen + iDataSegLen);
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "SendPdu: Send Request ");
		return -1;
	}

	return 0;
}


int
SendBody(
	IN SOCKET			connSock,
	IN PENCRYPTION_INFO	EncryptInfo,
	IN PNDASDIGEST_INFO	DigestInfo,
	IN ULONG			DataTransferLength,
	IN ULONG			DataBufferLength,
	IN PUCHAR			DataBuffer
){

	int		iResult;
	int		iTotalRecved = 0;

	UNREFERENCED_PARAMETER(DigestInfo);

	//
	//	Parameter check
	//

	if(DataBuffer == NULL)
		return -1;
	if(DataTransferLength == 0)
		return 0;
	if(DataBufferLength < DataTransferLength)
		return -1;

	//
	// Encrypt body
	//
	if(EncryptInfo && EncryptInfo->BodyEncryptAlgo != 0) {

			Encrypt32(
				DataBuffer,
				DataTransferLength,
				(unsigned char *)&EncryptInfo->CHAP_C,
				(unsigned char*)&EncryptInfo->Password64
				);
	}

	// Send Request.
	iResult = SendIt(
				connSock,
				DataBuffer,
				DataTransferLength);
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "SendBody: Send Request ");
		return -1;
	}

	return iResult;
}
