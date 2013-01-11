#pragma once

#include <winsock2.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include <ndas/ndascomm.h>

typedef enum _NDASCOMM_STATE {
	NDASCOMM_STATE_INVALID,
	NDASCOMM_STATE_CREATED,
	NDASCOMM_STATE_CONNECTED,
	NDASCOMM_STATE_LOGGED_IN,
} NDASCOMM_STATE;

typedef struct _NDASCOMM_HANDLE {

	NDASCOMM_STATE State;

	lsp_handle_t LspHandle;
	lsp_login_info_t LspLoginInfo;
	
	const lsp_hardware_data_t* LspHardwareData;
	const lsp_ata_handshake_data_t* LspAtaData;

	BYTE VendorId;
	DWORD Flags; /* Connection Flags */

	CRITICAL_SECTION ContextLock;
	DWORD LockCount[4]; /* To support nested lock */

	SOCKET s;
	HANDLE TransferEvent;
	DWORD SendTimeout;
	DWORD ReceiveTimeout;
	DWORD TransportProtocol;
	DWORD AddressType;
	PSOCKET_ADDRESS HostSockAddress;
	PSOCKET_ADDRESS DeviceSocketAddress;

	NDAS_DEVICE_ID NdasDeviceId;

	DWORD LspWriteBufferSize;
	PVOID LspWriteBuffer;

	DWORD LspSessionBufferSize;
	BYTE LspSessionBuffer[1];

} NDASCOMM_CONTEXT, *PNDASCOMM_CONTEXT;
