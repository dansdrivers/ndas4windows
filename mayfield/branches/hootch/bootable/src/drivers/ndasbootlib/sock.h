/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the AF_INET socket handler.
 *
 * Version:	@(#)sock.h	1.0.4	05/13/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Florian La Roche <flla@stud.uni-sb.de>
 *
 * Fixes:
 *		Alan Cox	:	Volatiles in skbuff pointers. See
 *					skbuff comments. May be overdone,
 *					better to prove they can be removed
 *					than the reverse.
 *		Alan Cox	:	Added a zapped field for tcp to note
 *					a socket is reset and must stay shut up
 *		Alan Cox	:	New fields for options
 *	Pauline Middelink	:	identd support
 *		Alan Cox	:	Eliminate low level recv/recvfrom
 *		David S. Miller	:	New socket lookup architecture.
 *              Steve Whitehouse:       Default routines for sock_ops
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef __SOCK_H
#define __SOCK_H

#include "debug.h"
#include "netdevice.h"
#include "skbuff.h"
#include "lpx.h"

/*
 * The idea is to start moving to a newer struct gradualy
 * 
 * IMHO the newer struct should have the following format:
 * 
 *	struct sock {
 *		sockmem [mem, proto, callbacks]
 *
 *		union or struct {
 *			ax25;
 *		} ll_pinfo;
 *	
 *		union {
 *			ipv4;
 *			ipv6;
 *			ipx;
 *			netrom;
 *			rose;
 * 			x25;
 *		} net_pinfo;
 *
 *		union {
 *			tcp;
 *			udp;
 *			spx;
 *			netrom;
 *		} tp_pinfo;
 *
 *	}
 */

struct sock {
	struct device *nic;	/* Bound device index if != 0		*/

	/* This must be first. */
	struct sock		*sklist_next;
	struct sock		*sklist_prev;

	/* Local port binding hash linkage. */
	struct sock		*bind_next;
	struct sock		**bind_pprev;

	/* Socket demultiplex comparisons on incoming packets. */
	ULONG			daddr;		/* Foreign IPv4 addr			*/
	ULONG			rcv_saddr;	/* Bound local IPv4 addr		*/
	USHORT			dport;		/* Destination port			*/
	unsigned short		num;		/* Local port				*/
	int			bound_dev_if;	/* Bound device index if != 0		*/

	/* Main hash linkage for various protocol lookup tables. */
	struct sock		*next;
//	struct sock		**pprev;

	volatile unsigned char	state,		/* Connection state			*/
				zapped;		/* In ax25 & ipx means not linked	*/
//	USHORT			sport;		/* Source port				*/

//	unsigned short		family;		/* Address family			*/
//	unsigned char		reuse,		/* SO_REUSEADDR setting			*/
//				nonagle;	/* Disable Nagle algorithm?		*/

	ULONG		sock_readers;	/* User count				*/
	KSPIN_LOCK	sock_spinlock; 
	KEVENT		ArrivalEvent;

	struct sk_buff_head	receive_queue;	/* Incoming packets			*/
	struct sk_buff_head	write_queue;	/* Packet sending queue			*/

	volatile char		dead,
				done,
				urginline,
				keepopen,
				linger,
				destroy,
				no_check,
				broadcast,
				bsdism;

	/* Error and backlog packet queues, rarely used. */
	struct sk_buff_head	back_log,
	                        error_queue;

//	struct proto		*prot;

	unsigned short		shutdown;

	union {
		struct lpx_stream_opt	af_lpx_stream;
		struct lpx_dgram_opt	af_lpx_dgram;
	} tp_pinfo;

	int			err, err_soft;	/* Soft holds errors that don't
						   cause failure but are the cause
						   of a persistent failure not just
						   'timed out' */
	unsigned short		ack_backlog;
	unsigned short		max_ack_backlog;
//	ULONG				priority;
	unsigned short		type;

	/* This is where all the private (optional) areas that don't
	 * overlap will eventually live. 
	 */

	union {
		struct lpx_opt	af_lpx;
	} protinfo;  		

	/* Callbacks */
	void			(*state_change)(struct sock *sk);
	void			(*data_ready)(struct sock *sk,int bytes);
//	void			(*write_space)(struct sock *sk);
	void			(*error_report)(struct sock *sk);

  	int				(*backlog_rcv) (struct sock *sk,
						struct sk_buff *skb);  
//	void                    (*destruct)(struct sock *sk);	
};

#define SHUTDOWN_MASK	3
#define RCV_SHUTDOWN	1
#define SEND_SHUTDOWN	2

/*
 * Used by processes to "lock" a socket state, so that
 * interrupts and bottom half handlers won't change it
 * from under us. It essentially blocks any incoming
 * packets, so that we won't get any new data or any
 * packets that change the state of the socket.
 */
extern	KSPIN_LOCK	sock_spinlock;

extern void __release_sock(struct sock *sk);

static void __inline lock_sock(struct sock *sk)
{
	KIRQL flags;

	KeAcquireSpinLock(&sock_spinlock, &flags);
	if(sk->sock_readers != 0 ) {
		NbDebugPrint(0, ("double lock on socket at %p\n", sk));
		while(1);
	}
	InterlockedIncrement(&sk->sock_readers);
//	NbDebugPrint(0, ("lock_sock: sk = %p, sock_readers = %d, sk->back_log.len = %d\n", sk, sk->sock_readers, sk->back_log.qlen));
	KeReleaseSpinLock(&sock_spinlock, flags);
}

static void __inline release_sock(struct sock *sk)
{	
	KIRQL flags;

	KeAcquireSpinLock(&sock_spinlock, &flags);
	if (InterlockedDecrement(&sk->sock_readers) == 0) {
//		NbDebugPrint(0, ("__release_sock: sk = %p, sock_readers = %d, sk->back_log.len = %d\n", sk, sk->sock_readers, sk->back_log.qlen));
		__release_sock(sk);
	}
	KeReleaseSpinLock(&sock_spinlock, flags);
}


extern struct sock *		sk_alloc(int family, int priority, int zero_it);
extern void			sk_free(struct sock *sk);
extern void			destroy_sock(struct sock *sk);

extern struct sk_buff		*sock_wmalloc(struct sock *sk,
					      unsigned long size, int force,
					      int priority);

extern struct sk_buff 		*sock_alloc_send_skb(struct sock *sk,
						     unsigned long size,
						     unsigned long fallback,
						     int noblock,
						     int *errcode);

/* Initialise core socket variables */
//extern void sock_init_data(struct socket *sock, struct sock *sk);
extern void sock_init_data(struct sock *sk, struct device *net_dev, unsigned short type);

/*
 * 	Queue a received datagram if it will fit. Stream and sequenced
 *	protocols can't normally use this as they need to fit buffers in
 *	and play with them.
 *
 * 	Inlined as it's very short and called for pretty much every
 *	packet ever received.
 */

extern __inline void skb_set_owner_r(struct sk_buff *skb, struct sock *sk)
{
	skb->sk = sk;
//	skb->destructor = sock_rfree;
//	atomic_add(skb->truesize, &sk->rmem_alloc);
}

extern __inline int sock_queue_rcv_skb(struct sock *sk, struct sk_buff *skb)
{
	/* Cast skb->rcvbuf to unsigned... It's pointless, but reduces
	   number of warnings when compiling with -W --ANK
	 */
//	if (atomic_read(&sk->rmem_alloc) + skb->truesize >= (unsigned)sk->rcvbuf)
//			return -ENOMEM;

	skb_set_owner_r(skb, sk);
	skb_queue_tail(&sk->receive_queue, skb);
	if (!sk->dead)
		sk->data_ready(sk,skb->len);
	return 0;
}

/*
 *	Recover an error report and clear atomically
 */
 
extern __inline int sock_error(struct sock *sk)
{
	int err;
	unsigned long flags = 0;
	
	err = sk->err;
	sk->err = 0;	

	return -err;
}

struct msghdr {
        void    *       msg_name;       /* Socket name                  */
        int             msg_namelen;    /* Length of name               */
        struct iovec *  msg_iov;        /* Data blocks                  */
        ULONG 			msg_iovlen;     /* Number of blocks             */
        void    *       msg_control;    /* Per protocol magic (eg BSD file descriptor passing) */
        ULONG			msg_controllen; /* Length of cmsg list */
        unsigned        msg_flags;
};


#define MSG_OOB		1
#define MSG_PEEK	2
#define MSG_DONTROUTE	4
#define MSG_TRYHARD     4       /* Synonym for MSG_DONTROUTE for DECnet */
#define MSG_CTRUNC	8
#define MSG_PROXY	0x10	/* Supply or ask second address. */
#define MSG_TRUNC	0x20
#define MSG_DONTWAIT	0x40	/* Nonblocking io		 */
#define MSG_EOR         0x80	/* End of record */
#define MSG_WAITALL	0x100	/* Wait for a full request */
#define MSG_FIN         0x200
#define MSG_SYN		0x400
#define MSG_URG		0x800
#define MSG_RST		0x1000
#define MSG_ERRQUEUE	0x2000
#define MSG_NOSIGNAL	0x4000

#define SOCK_STREAM    1		/* stream (connection) socket	*/
#define SOCK_DGRAM     2		/* datagram (conn.less) socket	*/
#define SOCK_RAW       3		/* raw socket			*/
#define SOCK_RDM       4		/* reliably-delivered message	*/
#define SOCK_SEQPACKET 5		/* sequential packet socket	*/
#define SOCK_PACKET    10		/* linux specific way of	*/


extern __inline int lpx_if_offset(struct sock *sk) 
{
	return sk->protinfo.af_lpx.interface->itf_dev->hard_header_len;
}

typedef struct sock *SOCK_HANDLE, **PSOCK_HANDLE;

#define		start_bh_atomic(x)			KeAcquireSpinLock(&sock_spinlock, &x)
#define		end_bh_atomic(x)			KeReleaseSpinLock(&sock_spinlock, x)

#define		ACQUIRE_SOCKLOCK(sk,flags)		KeAcquireSpinLock(&sk->sock_spinlock, &flags)
#define		RELEASE_SOCKLOCK(sk,flags)		KeReleaseSpinLock(&sk->sock_spinlock, flags)

extern void sock_init(void);

extern void lpx_proto_init(void);
extern void lpx_proto_finito(void);
extern void smp_proto_init(void);
extern void smp_proto_finito(void);

extern int lpx_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt);
extern int lpx_getname(SOCK_HANDLE sk, struct sockaddr *uaddr, int *uaddr_len);
extern int lpx_connect(SOCK_HANDLE sk, struct sockaddr *uaddr, int addr_len, int flags);
extern int lpx_bind(SOCK_HANDLE sk, struct sockaddr *uaddr, int addr_len);

extern SOCK_HANDLE smp_create(short type);
extern int smp_bind(SOCK_HANDLE sk, struct sockaddr *uaddr, int addr_len);
extern int smp_connect(SOCK_HANDLE sk, struct sockaddr *uaddr, int addr_len);
extern int smp_sendmsg(SOCK_HANDLE sk, void *msg, int len, int flags);
extern int smp_release(SOCK_HANDLE sk);
extern int smp_listen(SOCK_HANDLE sk, int backlog);
extern int smp_accept(SOCK_HANDLE sk, SOCK_HANDLE *newsk);
extern int smp_recvmsg(struct sock *sk, void *msg, int size);
extern void smp_rcv(struct sock *sk, struct sk_buff *skb);

#endif	/* __SOCK_H */
