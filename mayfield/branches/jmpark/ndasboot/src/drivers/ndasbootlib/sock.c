/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Generic socket support routines. Memory allocators, socket lock/release
 *		handler for protocols to use and generic option handler.
 *
 *
 * Version:	$Id: sock.c,v 1.80 1999/05/08 03:04:34 davem Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Florian La Roche, <flla@stud.uni-sb.de>
 *		Alan Cox, <A.Cox@swansea.ac.uk>
 *
 * Fixes:
 *		Alan Cox	: 	Numerous verify_area() problems
 *		Alan Cox	:	Connecting on a connecting socket
 *					now returns an error for tcp.
 *		Alan Cox	:	sock->protocol is set correctly.
 *					and is not sometimes left as 0.
 *		Alan Cox	:	connect handles icmp errors on a
 *					connect properly. Unfortunately there
 *					is a restart syscall nasty there. I
 *					can't match BSD without hacking the C
 *					library. Ideas urgently sought!
 *		Alan Cox	:	Disallow bind() to addresses that are
 *					not ours - especially broadcast ones!!
 *		Alan Cox	:	Socket 1024 _IS_ ok for users. (fencepost)
 *		Alan Cox	:	sock_wfree/sock_rfree don't destroy sockets,
 *					instead they leave that for the DESTROY timer.
 *		Alan Cox	:	Clean up error flag in accept
 *		Alan Cox	:	TCP ack handling is buggy, the DESTROY timer
 *					was buggy. Put a remove_sock() in the handler
 *					for memory when we hit 0. Also altered the timer
 *					code. The ACK stuff can wait and needs major 
 *					TCP layer surgery.
 *		Alan Cox	:	Fixed TCP ack bug, removed remove sock
 *					and fixed timer/inet_bh race.
 *		Alan Cox	:	Added zapped flag for TCP
 *		Alan Cox	:	Move kfree_skb into skbuff.c and tidied up surplus code
 *		Alan Cox	:	for new sk_buff allocations wmalloc/rmalloc now call alloc_skb
 *		Alan Cox	:	kfree_s calls now are kfree_skbmem so we can track skb resources
 *		Alan Cox	:	Supports socket option broadcast now as does udp. Packet and raw need fixing.
 *		Alan Cox	:	Added RCVBUF,SNDBUF size setting. It suddenly occurred to me how easy it was so...
 *		Rick Sladkey	:	Relaxed UDP rules for matching packets.
 *		C.E.Hawkins	:	IFF_PROMISC/SIOCGHWADDR support
 *	Pauline Middelink	:	identd support
 *		Alan Cox	:	Fixed connect() taking signals I think.
 *		Alan Cox	:	SO_LINGER supported
 *		Alan Cox	:	Error reporting fixes
 *		Anonymous	:	inet_create tidied up (sk->reuse setting)
 *		Alan Cox	:	inet sockets don't set sk->type!
 *		Alan Cox	:	Split socket option code
 *		Alan Cox	:	Callbacks
 *		Alan Cox	:	Nagle flag for Charles & Johannes stuff
 *		Alex		:	Removed restriction on inet fioctl
 *		Alan Cox	:	Splitting INET from NET core
 *		Alan Cox	:	Fixed bogus SO_TYPE handling in getsockopt()
 *		Adam Caldwell	:	Missing return in SO_DONTROUTE/SO_DEBUG code
 *		Alan Cox	:	Split IP from generic code
 *		Alan Cox	:	New kfree_skbmem()
 *		Alan Cox	:	Make SO_DEBUG superuser only.
 *		Alan Cox	:	Allow anyone to clear SO_DEBUG
 *					(compatibility fix)
 *		Alan Cox	:	Added optimistic memory grabbing for AF_UNIX throughput.
 *		Alan Cox	:	Allocator for a socket is settable.
 *		Alan Cox	:	SO_ERROR includes soft errors.
 *		Alan Cox	:	Allow NULL arguments on some SO_ opts
 *		Alan Cox	: 	Generic socket allocation to make hooks
 *					easier (suggested by Craig Metz).
 *		Michael Pall	:	SO_ERROR returns positive errno again
 *              Steve Whitehouse:       Added default destructor to free
 *                                      protocol private data.
 *              Steve Whitehouse:       Added various other default routines
 *                                      common to several socket families.
 *              Chris Evans     :       Call suser() check last on F_SETOWN
 *		Jay Schulist	:	Added SO_ATTACH_FILTER and SO_DETACH_FILTER.
 *		Andi Kleen	:	Add sock_kmalloc()/sock_kfree_s()
 *		Andi Kleen	:	Fix write_space callback
 *
 * To Fix:
 *
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */


#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include "ndasboot.h"
#include "linux2win.h"
#include "sock.h"

KSPIN_LOCK	sock_spinlock;

void sock_init(void)
{
	KeInitializeSpinLock(&sock_spinlock);
}

/*
 *	All socket objects are allocated here. This is for future
 *	usage.
 */
 
struct sock *sk_alloc(int family, int priority, int zero_it)
{
	struct sock *sk = kmalloc(sizeof(struct sock), GFP_KERNEL);

	UNREFERENCED_PARAMETER(family);
	UNREFERENCED_PARAMETER(priority);

	if(sk) {
		if (zero_it) 
			memset(sk, 0, sizeof(struct sock));
//		sk->family = family;
	}

	return sk;
}

void sk_free(struct sock *sk)
{
//	if (sk->destruct)
//		sk->destruct(sk);

	kfree(sk);
}

/*
 * Allocate a skb from the socket's send buffer.
 */
struct sk_buff *sock_wmalloc(struct sock *sk, unsigned long size, int force, int priority)
{
	struct sk_buff * skb = alloc_skb(size, priority);

	UNREFERENCED_PARAMETER(force);

	if (skb) {
//		skb->destructor = sock_wfree;
		skb->sk = sk;
		return skb;
	}

	return NULL;
}

/*
 *	Generic send/receive buffer handlers
 */

struct sk_buff *sock_alloc_send_skb(struct sock *sk, unsigned long size, 
			unsigned long fallback, int noblock, int *errcode)
{
	int err;
	struct sk_buff *skb;

	while (1) {
		unsigned long try_size = size;

		err = sock_error(sk);
		if (err != 0)
			goto failure;

		/*
		 *	We should send SIGPIPE in these cases according to
		 *	1003.1g draft 6.4. If we (the user) did a shutdown()
		 *	call however we should not. 
		 *
		 *	Note: This routine isnt just used for datagrams and
		 *	anyway some datagram protocols have a notion of
		 *	close down.
		 */

//		err = -EPIPE;
//		if (sk->shutdown&SEND_SHUTDOWN)
//			goto failure;

		if (fallback) {
			/* The buffer get won't block, or use the atomic queue.
			 * It does produce annoying no free page messages still.
			 */
			skb = sock_wmalloc(sk, size, 0, 0);
//			if (skb)
				break;
			try_size = fallback;
		}

		skb = sock_wmalloc(sk, try_size, 0, 0);
		if (skb)
			break;

		/*
		 *	This means we have too many buffers for this socket already.
		 */

		if (noblock)
			goto failure;
	}

	return skb;

failure:
	*errcode = err;
	return NULL;
}

void __release_sock(struct sock *sk)
{
	KIRQL	oldIrql;

//	if (!sk->prot || !sk->backlog_rcv)
//		return;
	/* See if we have any packets built up. */
	
	while (!skb_queue_empty(&sk->back_log)) {
		struct sk_buff * skb = sk->back_log.next;
		__skb_unlink(skb, &sk->back_log);
		sk->backlog_rcv(sk, skb);
	}
//	DbgPrint("__release_sock end\n");	
}


/*
 *	Generic socket manager library. Most simpler socket families
 *	use this to manage their socket lists. At some point we should
 *	hash these. By making this generic we get the lot hashed for free.
 */
 
void sklist_remove_socket(struct sock **list, struct sock *sk)
{
	struct sock *s;
	KIRQL	oldflags;

	start_bh_atomic(oldflags);

	s= *list;
	if(s==sk)
	{
		*list = s->next;
		end_bh_atomic(oldflags);
		return;
	}
	while(s && s->next)
	{
		if(s->next==sk)
		{
			s->next=sk->next;
			break;
		}
		s=s->next;
	}
	end_bh_atomic(oldflags);
}

void sklist_insert_socket(struct sock **list, struct sock *sk)
{
	KIRQL	oldflags;

	start_bh_atomic(oldflags);
	sk->next= *list;
	*list=sk;
	end_bh_atomic(oldflags);
}

/*
 *	This is only called from user mode. Thus it protects itself against
 *	interrupt users but doesn't worry about being called during work.
 *	Once it is removed from the queue no interrupt or bottom half will
 *	touch it and we are (fairly 8-) ) safe.
 */

void sklist_destroy_socket(struct sock **list, struct sock *sk);

/*
 *	Handler for deferred kills.
 */

static void sklist_destroy_timer(unsigned long data)
{
	struct sock *sk=(struct sock *)data;
	sklist_destroy_socket(NULL,sk);
}

/*
 *	Destroy a socket. We pass NULL for a list if we know the
 *	socket is not on a list.
 */
 
/*
 *	Default Socket Callbacks
 */

void sock_def_wakeup(struct sock *sk)
{	
	if(!sk->dead) {		
		KeSetEvent(&sk->ArrivalEvent, IO_NO_INCREMENT, FALSE);	
	}
}


void sock_def_error_report(struct sock *sk)
{
	if (!sk->dead) {
		KeSetEvent(&sk->ArrivalEvent, IO_NO_INCREMENT, FALSE);
	}
}


void sock_def_readable(struct sock *sk, int len)
{
	UNREFERENCED_PARAMETER(len);

	if(!sk->dead) {
		KeSetEvent(&sk->ArrivalEvent, IO_NO_INCREMENT, FALSE);
	}
}

//void sock_init_data(struct socket *sock, struct sock *sk)
void sock_init_data(struct sock *sk, struct device *net_dev, unsigned short type)
{
	skb_queue_head_init(&sk->receive_queue);
	skb_queue_head_init(&sk->write_queue);
	skb_queue_head_init(&sk->back_log);
	skb_queue_head_init(&sk->error_queue);
	
	KeInitializeSpinLock(&sk->sock_spinlock);
	KeInitializeEvent(&sk->ArrivalEvent, NotificationEvent, FALSE);

//	init_timer(&sk->timer);
	
//	sk->allocation	=	GFP_KERNEL;
//	sk->rcvbuf	=	sysctl_rmem_default;
//	sk->sndbuf	=	sysctl_wmem_default;
	sk->state 	= 	0;
	sk->zapped	=	1;
//	sk->socket	=	sock;
//	sk->zapped	=	0;
	sk->nic		=	net_dev;
	sk->type	=	type;

	sk->state_change	=	sock_def_wakeup;
	sk->data_ready		=	sock_def_readable;
	sk->error_report	=	sock_def_error_report;
}