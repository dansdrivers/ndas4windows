/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASCMDPROC_H_
#define _NDASCMDPROC_H_
#include "transport.h"

class CNdasCommandProcessor
{
protected:
	ITransport* m_pTransport;

	OVERLAPPED m_overlapped;
	HANDLE m_hEvent;

	NDAS_CMD_TYPE m_command;
	NDAS_CMD_HEADER m_requestHeader;

	BOOL InitOverlapped();

	BOOL ReadPacket(
		LPVOID lpBuffer, 
		DWORD cbToRead, 
		LPDWORD lpcbRead);

	BOOL WritePacket(
		LPCVOID lpBuffer, 
		DWORD cbToWrite, 
		LPDWORD lpcbWritten);

	BOOL ReadHeader();

	BOOL WriteHeader(
		NDAS_CMD_STATUS opStatus, 
		DWORD cbDataSize);

	BOOL WriteErrorReply(
		NDAS_CMD_STATUS opStatus,
		DWORD dwErrorCode = 0,
		DWORD cbDataLength = 0,
		LPVOID lpData = NULL);

	template <typename T>
	BOOL ProcessCommandRequest(T* pT, DWORD cbData);

public:

	CNdasCommandProcessor(ITransport* pTransport);
	virtual ~CNdasCommandProcessor();

	BOOL Process();

};

#endif
