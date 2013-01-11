#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include "ndasboot.h"
#include "lanscsi.h"
#include "pci.h"
#include "sock.h"
#include "skbuff.h"
#include "netdevice.h"
#include "lpx.h"
#include "iov.h"

extern int link_initialized;

int NDASBootInitialize(void)
{
		ULONG ret; 

		pci_init();
		sock_init();
		skbuff_init();		
		
		ret = net_dev_init();		
		if(ret < 0) {
			return ret;
		}
		lpx_proto_init();		
	
		return 0; 
}

int NDASBootDestroy(void)
{
		net_dev_destroy();

		return 0;
}

int NetProtoReInitialize(void)
{
		ULONG ret;

		NbDebugPrint(1, ("NetProtoReInitialize\n"));

		net_dev_destroy();

		sock_init();
		skbuff_init();		
		ret = net_dev_init();
		if(ret < 0) {
			return ret;
		}
		lpx_proto_init();
	
		return 0; 
}

PUCHAR GetNICAddr(VOID)
{
	return NetDevice.dev_addr;
}

//
//

int GetNICStatus(void)
{
	//
	// TODO:
	// When it gets called by lpx_stream_timer(), a timer routine, during NIC probe,
	// this if-statement temporarily fixes NULL memory reference.
	// should find another way to prevent calling this during NIC probe.
	//

	if(NetDevice.get_status == NULL) {
		NbDebugPrint(1, ("GetNICStatus: No get_status interface.\n"));
		return STATUS_NIC_NOT_IMPL;
	}

	return NetDevice.get_status(&NetDevice);
}

//////////////////////////////////////////////////////////
//					LPX TDI functions					//
//////////////////////////////////////////////////////////

NTSTATUS
LpxConnect(
		IN	PTA_LSTRANS_ADDRESS		Address,
		OUT	PVOID					*pSock
)
{
		int						result;
		struct sockaddr_lpx		slpx;
		struct sockaddr_lpx		addr;
		SOCK_HANDLE				sock;

		NbDebugPrint(1, ("LpxConnect Entered\n"));

		if(NULL == pSock) {
			NbDebugPrint(0, ("Invalid parater pSock \n"));
			return STATUS_INVALID_PARAMETER;
		}
		
		sock = lpx_create(SOCK_STREAM, 0);

		if( sock < 0) {
			NbDebugPrint(0, ("failed to create socket!\n"));			
			return STATUS_UNSUCCESSFUL;
		}		
	
		memset(&slpx, 0, sizeof(slpx));
		slpx.slpx_family = AF_LPX;

#if 0
		slpx.slpx_port = 0;
		slpx.slpx_node[0] = 0x00;
		slpx.slpx_node[1] = 0x50;
		slpx.slpx_node[2] = 0x04;
		slpx.slpx_node[3] = 0xc0;
		slpx.slpx_node[4] = 0x46;
		slpx.slpx_node[5] = 0xB5;
#endif
	
		if( (result = lpx_stream_bind(sock, (struct sockaddr *)&slpx, sizeof(slpx))) < 0) {
			NbDebugPrint(0, ("failed to bind socket!\n"));
			lpx_stream_release(sock, NULL);
			return STATUS_UNSUCCESSFUL;
		}

		addr.slpx_family = AF_LPX;
		addr.slpx_port = (USHORT)(*((USHORT*)&Address->Address[0].Address.Address[0]));
		memcpy(addr.slpx_node, &Address->Address[0].Address.Address[2], LPX_NODE_LEN);
		
		result = lpx_stream_connect((SOCK_HANDLE)sock, (struct sockaddr *)&addr, sizeof(addr), 0);
		
		if(result < 0) {
			NbDebugPrint(0, ("failed to connect socket!\n"));
			lpx_stream_release(sock, NULL);
			return STATUS_UNSUCCESSFUL;
		}

		*pSock = sock;

		return STATUS_SUCCESS;
}

NTSTATUS
LpxDisConnect(	
		IN	PVOID					Sock
)
{
		NbDebugPrint(1, ("LpxDisConnect Entered\n"));

		if(NULL == Sock) {
			NbDebugPrint(0, ("Invalid parater pSock \n"));
			return STATUS_INVALID_PARAMETER;
		}
		lpx_stream_release((SOCK_HANDLE)Sock, NULL);		

		return STATUS_SUCCESS; 
}

NTSTATUS
LpxReceive(
	   IN	PVOID			Sock,
	   IN	PCHAR			buf, 
	   IN	int				size,
	   IN	ULONG			flags, 
	   IN	PLARGE_INTEGER	TimeOut,
	   OUT	PLONG			result

	   )
{
		struct msghdr msg;
		struct iovec iov;
		NTSTATUS ntStatus;		

		NbDebugPrint(1, ("LpxReceive: buf = %lx, size = %lx, flags = %x\n", buf, size, flags));

		iov.iov_base = buf;
		iov.iov_len = size;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = flags;

		*result = lpx_stream_recvmsg((SOCK_HANDLE)Sock, &msg, size, 0, TimeOut);

		if(*result < 0) {
			ntStatus = STATUS_CONNECTION_DISCONNECTED;
		}
		else {
			ntStatus = STATUS_SUCCESS;
		}

		return ntStatus;
}

NTSTATUS
LpxSend(
	   PVOID			Sock,
	   PCHAR			buf, 
	   int				size,
	   ULONG			flags,	   
	   PULONG			sent_len
	   )
{
		struct msghdr msg;
		struct iovec iov;
		NTSTATUS ntStatus;
		ULONG result;
		
		NbDebugPrint(1, ("LpxSend: buf = %lx, size = %lx, flags = %x\n", buf, size, flags));
		
		iov.iov_base = buf;
		iov.iov_len = size;

		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = flags;

		result = lpx_stream_sendmsg((SOCK_HANDLE)Sock, &msg, size);

		if(result < 0) {
			ntStatus = STATUS_CONNECTION_DISCONNECTED;
		}
		else {
			*sent_len = result;
			ntStatus = STATUS_SUCCESS;
		}

		return ntStatus;
}	
