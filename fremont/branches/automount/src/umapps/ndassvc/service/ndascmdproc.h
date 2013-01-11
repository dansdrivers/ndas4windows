/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASCMDPROC_H_
#define _NDASCMDPROC_H_
#include "transport.h"
#include <xtl/xtlautores.h>

class CNdasService;

class CNdasCommandProcessor
{
protected:
	CNdasService& m_service;
	ITransport* m_pTransport;

	XTL::AutoObjectHandle m_hEvent;
	OVERLAPPED m_overlapped;

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
		LPDWORD lpcbWritten = NULL);

	BOOL ReadHeader();

	BOOL WriteHeader(
		NDAS_CMD_STATUS opStatus, 
		DWORD cbDataSize);

	BOOL ReplyError(
		NDAS_CMD_STATUS opStatus,
		DWORD dwErrorCode = 0,
		DWORD cbDataLength = 0,
		LPVOID lpData = NULL);

	BOOL
	ReplySimple(
		NDAS_CMD_STATUS status,
		LPCVOID lpData,
		DWORD cbData,
		LPDWORD lpcbWritten = NULL);

	BOOL WriteSuccessHeader(DWORD cbDataSize)
	{
		return WriteHeader(NDAS_CMD_STATUS_SUCCESS, cbDataSize);
	}

	BOOL ReplySuccessDummy()
	{
		return ReplySimple(NDAS_CMD_STATUS_SUCCESS, 0, 0, 0);
	}

	BOOL ReplySuccessSimple(LPCVOID lpData, DWORD cbData, LPDWORD lpcbWritten = NULL)
	{
		return ReplySimple(NDAS_CMD_STATUS_SUCCESS, lpData, cbData, lpcbWritten);
	}

	template <typename T>
	BOOL ReplySuccessSimple(const T* pReply)
	{
		return ReplySimple(NDAS_CMD_STATUS_SUCCESS, pReply, pReply->Size(), NULL);
	}

	BOOL ReplyCommandFailed(DWORD dwErrorCode, DWORD cbDataLength = 0, LPVOID lpData = NULL)
	{
		return ReplyError(NDAS_CMD_STATUS_FAILED, dwErrorCode, cbDataLength, lpData);
	}

	template <typename T>
	BOOL ProcessCommandRequest(T* pT, DWORD cbData);

public:

	CNdasCommandProcessor(CNdasService& service, ITransport* pTransport);
	~CNdasCommandProcessor();

	BOOL Process();

};

#endif
