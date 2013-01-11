#pragma once

HRESULT
NdasCommTransportConnect(
	__out SOCKET* sock,
	__in INT Protocol,
	__in const SOCKET_ADDRESS * DeviceSocketAddress,
	__in_opt const SOCKET_ADDRESS_LIST* SourceSocketAddressList);

HRESULT
NdasCommTransportLspRequest(
	__in HNDAS NdasHandle,
	__inout lsp_status_t* LspStatus,
	__in LPOVERLAPPED Overlapped);
