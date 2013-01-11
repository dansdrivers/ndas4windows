#include <winsock2.h>
#include <crtdbg.h>
#include <stdlib.h>
#include <lspx/lsp.h>
#include <xtl/xtldef.h>
#include <ndas/ndasmsg.h>
#include "ndascommp.h"
#include "ndascommtransport.h"

HRESULT
NdasCommTransportpGetHostAddressList(
	__deref_out LPSOCKET_ADDRESS_LIST* SocketAddressList,
	__in INT AddressFamily,
	__in INT Type,
	__in INT Protocol);

HRESULT
NdasCommTransportpGetHostAddressList(
	__deref_out LPSOCKET_ADDRESS_LIST* SocketAddressList,
	__in SOCKET sock);

HRESULT
NdasCommTransportpGetHostAddressList(
	__deref_out LPSOCKET_ADDRESS_LIST* SocketAddressList,
	__in INT AddressFamily,
	__in INT Type,
	__in INT Protocol)
{
	HRESULT hr;

	SOCKET s = socket(AddressFamily, Type, Protocol);

	if (INVALID_SOCKET == (SOCKET) s)
	{
		hr = HRESULT_FROM_WIN32(WSAGetLastError());
		return hr;
	}

	hr = NdasCommTransportpGetHostAddressList(SocketAddressList, s);

	XTLVERIFY( 0 == closesocket(s) );

	return hr;
}

HRESULT
NdasCommTransportpGetHostAddressList(
	__deref_out LPSOCKET_ADDRESS_LIST* SocketAddressList,
	__in SOCKET sock)
{
	HRESULT hr;

	*SocketAddressList = NULL;

	//
	// Query Buffer length should not affect last error
	//
	DWORD socketAddressListLength = 0;
	
	LPSOCKET_ADDRESS_LIST socketAddressList = 
		static_cast<LPSOCKET_ADDRESS_LIST>(malloc(socketAddressListLength));

	if (NULL == socketAddressList)
	{
		return E_OUTOFMEMORY;
	}

	while (TRUE)
	{
		DWORD savedError = GetLastError();

		int sockret;

		sockret = WSAIoctl(
			sock, 
			SIO_ADDRESS_LIST_QUERY, 
			0, 0, 
			socketAddressList,
			socketAddressListLength, 
			&socketAddressListLength, 
			NULL, NULL);

		if (sockret != SOCKET_ERROR)
		{
			SetLastError(savedError);
			*SocketAddressList = socketAddressList;
			return S_OK;
		}

		if (WSAEFAULT != WSAGetLastError())
		{
			hr = HRESULT_FROM_WIN32(WSAGetLastError());
			free(socketAddressList);
			return hr;
		}

		SetLastError(savedError);

		PVOID p = realloc(socketAddressList, socketAddressListLength);

		if (NULL == p)
		{
			free(socketAddressList);
			return E_OUTOFMEMORY;
		}

		socketAddressList = (LPSOCKET_ADDRESS_LIST) p;
	}
}

HRESULT
NdasCommTransportConnect(
	__out SOCKET* Socket,
	__in INT Protocol,
	__in const SOCKET_ADDRESS * DeviceSocketAddress,
	__in_opt const SOCKET_ADDRESS_LIST* SourceSocketAddressList)
{
	HRESULT hr;
	int ret;
	LPSOCKET_ADDRESS_LIST addrList = NULL;

	DWORD nbcount = 0;
	SOCKET nbs[FD_SETSIZE];
	SOCKET connSocket = INVALID_SOCKET;

	*Socket = INVALID_SOCKET;

	//
	// Preallocate ConnectedSourceAddress
	//

	//
	// Maximum concurrent connection attempts are bound to FD_SETSIZE
	//

	if (NULL == SourceSocketAddressList)
	{
		hr = NdasCommTransportpGetHostAddressList(
			&addrList,
			DeviceSocketAddress->lpSockaddr->sa_family,
			SOCK_STREAM, 
			Protocol);

		if (FAILED(hr))
		{
			return hr;
		}

		SourceSocketAddressList = addrList;
	}

	fd_set writefds, exceptfds;

	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);

	for (int i = 0; i < SourceSocketAddressList->iAddressCount; ++i)
	{
		const SOCKET_ADDRESS* srcSocketAddr = 
			&SourceSocketAddressList->Address[i];
		
		SOCKET s = WSASocket(
			srcSocketAddr->lpSockaddr->sa_family,
			SOCK_STREAM,
			Protocol,
			NULL, 
			NULL,
			WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == s)
		{
			continue;
		}

		ret = bind(s, 
			srcSocketAddr->lpSockaddr, 
			srcSocketAddr->iSockaddrLength);

		if (SOCKET_ERROR == ret)
		{
			XTLVERIFY( 0 == closesocket(s) );
			continue;
		}

		ULONG nonblocking = 1;
		ret = ioctlsocket(s, FIONBIO, &nonblocking);

		if (SOCKET_ERROR == ret)
		{
			XTLVERIFY( 0 == closesocket(s) );
			continue;
		}

		ret = WSAConnect(s, 
			DeviceSocketAddress->lpSockaddr,
			DeviceSocketAddress->iSockaddrLength,
			NULL,
			NULL,
			NULL,
			NULL);

		if (0 == ret)
		{
			//
			// connect is completed immediately.
			//
			ULONG nonblocking = 0;
			XTLVERIFY( 0 == ioctlsocket(s, FIONBIO, &nonblocking) );

			connSocket = s;

			break;
		}
		if (SOCKET_ERROR == ret && WSAEWOULDBLOCK == WSAGetLastError())
		{
			nbs[nbcount] = s;
			++nbcount;

			FD_SET(s, &writefds);
			FD_SET(s, &exceptfds);
		}
		else
		{
			XTLVERIFY( 0 == closesocket(s) );	
		}
	}

	//
	// if connect is pending, select
	//

	if (INVALID_SOCKET != connSocket)
	{
		for (UINT i = 0; i < nbcount; ++i)
		{
			_ASSERT(INVALID_SOCKET != nbs[i]);
			CancelIo(reinterpret_cast<HANDLE>(nbs[i]));
			XTLVERIFY( 0 == closesocket(nbs[i]) );
		}
		free(addrList);
		*Socket = connSocket; 
		return S_OK;
	}

	while (writefds.fd_count > 0)
	{
		ret = select(1, NULL, &writefds, &exceptfds, NULL);

		if (SOCKET_ERROR == ret)
		{
			hr = HRESULT_FROM_WIN32(WSAGetLastError());
			for (UINT i = 0; i < nbcount; ++i)
			{
				if (INVALID_SOCKET != nbs[i])
				{
					CancelIo(reinterpret_cast<HANDLE>(nbs[i]));
					XTLVERIFY( 0 == closesocket(nbs[i]) );
				}
			}
			free(addrList);
			return hr;
		}

		for (UINT i = 0; i < nbcount; ++i)
		{
			if (INVALID_SOCKET == nbs[i])
			{
				continue;
			}
			if (FD_ISSET(nbs[i], &exceptfds))
			{
				XTLVERIFY( 0 == closesocket(nbs[i]) );
				FD_CLR(nbs[i], &writefds);
				FD_CLR(nbs[i], &exceptfds);
			}
			else if (FD_ISSET(nbs[i], &writefds))
			{
				connSocket = nbs[i];
				FD_CLR(connSocket, &writefds);
				FD_CLR(connSocket, &exceptfds);
				nbs[i] = INVALID_SOCKET;

				for (UINT j = 0; j < nbcount; ++j)
				{
					if (INVALID_SOCKET != nbs[i])
					{
						CancelIo(reinterpret_cast<HANDLE>(nbs[i]));
						XTLVERIFY(0 == closesocket(nbs[i]));
					}
				}

				free(addrList);

				ULONG nonblocking = 0;
				XTLVERIFY( 0 == ioctlsocket(connSocket, FIONBIO, &nonblocking) );

				*Socket = connSocket;
				return S_OK;
			}
		}
	}

	hr = HRESULT_FROM_WIN32(WSAGetLastError());
	free(addrList);
	return hr;
}

HRESULT
NdasCommTransportLspRequest(
	__in PNDASCOMM_CONTEXT Context,
	__inout lsp_status_t* LspStatus,
	__in LPOVERLAPPED Overlapped)
{
	HRESULT hr;
	WSABUF wsabuf;
	DWORD bytesTransferred;

	for (;; *LspStatus = lsp_process_next(Context->LspHandle))
	{
		switch (*LspStatus)
		{
		case LSP_REQUIRES_SEND:
		case LSP_REQUIRES_RECEIVE:
			{
				lsp_uint32_t buflen;

				if (LSP_REQUIRES_SEND == *LspStatus)
				{
					wsabuf.buf = (PCHAR) lsp_get_buffer_to_send(
						Context->LspHandle, &buflen);
					wsabuf.len = buflen;
				}
				else
				{
					wsabuf.buf = (PCHAR) lsp_get_buffer_to_receive(
						Context->LspHandle, &buflen);
					wsabuf.len = buflen;
				}

				DWORD txFlags = 0;

				WSAOVERLAPPED overlapped = {0};
				overlapped.hEvent = Context->TransferEvent;

				int ret;
				DWORD transferTimeout;

				if (LSP_REQUIRES_SEND == *LspStatus)
				{
					ret = WSASend(
						Context->s, 
						&wsabuf,
						1,
						&bytesTransferred,
						txFlags,
						&overlapped,
						NULL);
					transferTimeout = Context->SendTimeout;
				}
				else
				{
					ret = WSARecv(
						Context->s, 
						&wsabuf,
						1,
						&bytesTransferred,
						&txFlags,
						&overlapped,
						NULL);
					transferTimeout = Context->ReceiveTimeout;
				}

				if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
				{
					hr = HRESULT_FROM_WIN32(WSAGetLastError());
					return hr;
				}

				DWORD waitResult = WaitForSingleObject(
					Context->TransferEvent, 
					transferTimeout);

				_ASSERT(WAIT_OBJECT_0 == waitResult || 
					WAIT_TIMEOUT == waitResult);

				if (WAIT_OBJECT_0 != waitResult)
				{
					switch (waitResult)
					{
					case WAIT_TIMEOUT:
						hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
						break;
					case WAIT_FAILED:
						hr = HRESULT_FROM_WIN32(GetLastError());
						break;
					default:
						hr = E_FAIL;
						break;
					}
					shutdown(Context->s, SD_BOTH);
					return hr;
				}

				BOOL success = WSAGetOverlappedResult(
					Context->s,
					&overlapped,
					&bytesTransferred,
					TRUE,
					&txFlags);

				if (!success)
				{
					hr = HRESULT_FROM_WIN32(GetLastError());
					shutdown(Context->s, SD_BOTH);
					return hr;
				}
			}
			break;

		case LSP_REQUIRES_SYNCHRONIZE:

			break;

		default:
			if (*LspStatus == LSP_STATUS_SUCCESS)
			{
				return S_OK;
			}
			else
			{
				hr = ERROR_SEVERITY_ERROR | APPLICATION_ERROR_MASK | 
					0x0CC00000 | *LspStatus;
				return hr;
			}
		}
	}
}
