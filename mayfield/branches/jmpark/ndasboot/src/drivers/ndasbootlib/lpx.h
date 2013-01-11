#ifndef _NET_LPX_H_
#define _NET_LPX_H_

#include <pshpack1.h>

#include "skbuff.h"

typedef unsigned short sa_family_t;

#ifndef AF_LPX
#define	AF_LPX          30      /* NetKingCall LPX */ 
#ifndef PF_LPX
#define PF_LPX          AF_LPX
#endif
#endif

#define NANO100_PER_SEC			(LONGLONG)(10 * 1000 * 1000)

#ifdef __ENABLE_LOADER__
#define HZ						40
#else
#define HZ						NANO100_PER_SEC
#endif

#define HTONS(Data)		((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))
#define NTOHS(Data)		(USHORT)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))

#define HTONL(Data)		( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
						| (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))
#define NTOHL(Data)		( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
						| (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))

#define HTONLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define NTOHLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define IOCTL_DEL_TIMER		0x00000001

#define	LPX_NODE_LEN	6

#define __u8	UCHAR
#define __u16	USHORT
#define __u32	ULONG

#define spinlock_t	KSPIN_LOCK

struct sockaddr_lpx {
	sa_family_t			slpx_family;
	unsigned short    	slpx_port;
	unsigned char		slpx_node[LPX_NODE_LEN];
};


#define LPX_MIN_EPHEMERAL_SOCKET	0x4000
#define LPX_MAX_EPHEMERAL_SOCKET	0x7fff

//#define LPX_BROADCAST_NODE	"FFFFFFFFFFFF"
#define LPX_BROADCAST_NODE	"\xFF\xFF\xFF\xFF\xFF\xFF"

typedef	struct _lpx_address {
	__u16	port;
	__u8	node[LPX_NODE_LEN];
} lpx_address;

typedef struct _lpx_interface {
	struct device	*itf_dev;
	struct _lpx_interface	*itf_next;
	struct sock		*itf_sklist;
	unsigned char 		itf_node[LPX_NODE_LEN];
	unsigned short		itf_sknum;
} lpx_interface;

struct lpx_opt {
		lpx_interface		*interface;
        lpx_address             dest_addr;
        lpx_address             source_addr;
		__u16			virtual_mapping;  
};

///////////////////////////////////////////////////////////////
//		list used by port allocation 	
#define	PORT_HASH	73
#define	ADDR_HASH	30
#define LPX_CASH_TIMEOUT	

// socket binding list
struct lpx_bindhead {
	spinlock_t bind_lock;
	struct sock * bind_next;
};

// socket binding management structure
struct lpx_portinfo {
	spinlock_t	portalloc_lock;
	unsigned int last_alloc_port;
	struct lpx_bindhead port_hashtable[PORT_HASH];
};


struct lpx_cache{
	struct lpx_cache * next;
	LONGLONG	time;
	unsigned char dest_addr[LPX_NODE_LEN];
	unsigned char itf_addr[LPX_NODE_LEN];
};


// lpx_eth addr cache management structure
struct lpx_eth_cache {
	spinlock_t	cache_lock;
	KTIMER	cache_timer;
	KDPC cache_dpc;
	struct lpx_cache 	*head;
};

#define CACHE_ALIVE			(10*HZ)
#define CACHE_TIMEOUT		(5*HZ)
///////////////////////////////////////////////////////////////

struct lpx_dgram_opt {
	unsigned short		message_id;
	struct sk_buff_head     receive_packet_queue;
	unsigned short		receive_data_size;
#define	MAX_RECEIVE_DATA_SIZE	0x100000
};

/* 16 bit align  total  16 bytes */

struct	lpxhdr 
{
	union {
		struct {
			__u8	pktsizeMsb:6;
			__u8	type:2;
#define LPX_TYPE_RAW		0x0
#define LPX_TYPE_DATAGRAM	0x2
#define LPX_TYPE_STREAM		0x3	

			__u8	pktsizeLsb;
		} p;
#define LPX_TYPE_MASK		(unsigned short)0x00C0	

		__u16	pktsize;
	} pu;
	

	__u16	dest_port;
	__u16	source_port;

	union {
		struct {
			__u16	reserved_r[5];
		} r;
			
		struct {
			__u16	message_id;
			__u16	message_length;
			__u16	fragment_id;
			__u16	fragment_length;
			__u16	reserved_d3;
		}d;
		struct {
			__u16	lsctl;

/* Lpx Stream Control bits */
#define LSCTL_CONNREQ		0x0001
#define LSCTL_DATA		0x0002
#define LSCTL_DISCONNREQ	0x0004
#define LSCTL_ACKREQ		0x0008
#define LSCTL_ACK		0x1000

			__u16	sequence;
			__u16	ackseq;
//			__u16	window_size; /* not implemented */
//			__u16	reserved_s1;
			__u8	server_tag;
			__u8	reserved_s1[3];
		}s;
	}u;

};
	
#define	LPX_PKT_LEN	(sizeof(struct lpxhdr))

/*	HZ = 100	*/

/*
#define MAX_CONNECT_TIME	(1*HZ)

#define SMP_TIMEOUT		(HZ/20)
#define SMP_DESTROY_TIMEOUT	(1*HZ)
#define TIME_WAIT_INTERVAL	(HZ)
#define ALIVE_INTERVAL  	(HZ/2)

#define RETRANSMIT_TIME		(HZ/5)
#define MAX_RETRANSMIT_DELAY	(HZ/1)

#define MAX_ALIVE_COUNT     	(5 * HZ/ALIVE_INTERVAL)
#define MAX_RETRANSMIT_TIME    	(5 * HZ)
#define MAX_RETRANSMIT_COUNT    (MAX_RETRANSMIT_TIME/MAX_RETRANSMIT_DELAY)
#define MAX_CONNECT_COUNT	(MAX_CONNECT_TIME/MAX_RETRANSMIT_DELAY)
*/

#define MAX_CONNECT_TIME		(2*HZ)

#define SMP_TIMEOUT				(HZ/4)
#define SMP_DESTROY_TIMEOUT		(2*HZ)
#define TIME_WAIT_INTERVAL		(HZ)
#define ALIVE_INTERVAL  		(HZ/2)

#define RETRANSMIT_TIME			(HZ/2)
#define MAX_RETRANSMIT_DELAY	(HZ/1)

#define MAX_ALIVE_COUNT     	(2 * HZ/ALIVE_INTERVAL)
#define MAX_RETRANSMIT_TIME    	(2 * HZ)
#define MAX_RETRANSMIT_COUNT    (MAX_RETRANSMIT_TIME/MAX_RETRANSMIT_DELAY)
#define MAX_CONNECT_COUNT		(MAX_CONNECT_TIME/MAX_RETRANSMIT_DELAY)

struct lpx_stream_opt
{	
	void	*owner;

	struct sock		*child_sklist;
	struct sock		*parent_sk;

	__u16	sequence;	/* Host order - our current pkt # */
	__u16	fin_seq;

	__u16	rmt_seq;
	__u16	rmt_ack;	/* Host order - last pkt ACKd by remote */

	__u16	alloc;		/* Host order - max seq we can rcv now */
	__u16	rmt_alloc;	/* Host order - max seq remote can handle now */

	__u16	source_connid;	/* Net order */
	__u16	dest_connid;	/* Net order */

	__u8	server_tag;

	KTIMER IopTimer;
	LARGE_INTEGER deltaTime;
	
	__u32	timer_reason;
#define	SMP_SENDIBLE		0x0001
#define	SMP_RETRANSMIT_ACKED	0x0002
	LONGLONG	time_wait_timeout;
	LONGLONG	alive_timeout;
	LONGLONG	retransmit_timeout;
	__u16	last_retransmit_seq;

	int	alive_retries;	/* Number of WD retries */
	int	retransmits;	/* Number of retransmits */

	__u16	success_transmits;

#define SMP_MAX_FLIGHT 1024
	__u16	max_flights;

	LONGLONG	latest_sendtime;
	LONGLONG	interval_time;


	struct sk_buff_head     retransmit_queue;
};


/* Connection state defines */

enum {
  SMP_ESTABLISHED = 1,
  SMP_SYN_SENT,
  SMP_SYN_RECV,
  SMP_FIN_WAIT1,
  SMP_FIN_WAIT2,
  SMP_TIME_WAIT,
  SMP_CLOSE,
  SMP_CLOSE_WAIT,
  SMP_LAST_ACK,
  SMP_LISTEN,
  SMP_CLOSING,	 /* now a valid state */

  SMP_MAX_STATES /* Leave at the end! */
};

/* Packet transmit types - Internal */
#define DATA	0	/* Data */
#define ACK	1	/* Data ACK */
#define	CONREQ	4	/* Connection Request */
#define	DISCON	6	/* Informed Disconnect */
#define RETRAN	8	/* Int. Retransmit of packet */
#define ACKREQ	9	/* Int. Retransmit of packet */

extern void lpx_proto_init(void);
extern struct sock *lpx_create(int type, int protocol);

extern int lpx_stream_sendmsg(struct sock *sock, struct msghdr *msg, int len);
extern int lpx_stream_recvmsg(struct sock *sock, struct msghdr *msg, int size, int flags, PLARGE_INTEGER	TimeOut);
extern int  lpx_stream_bind(struct sock *sock, struct sockaddr *uaddr, int addr_len);
extern int lpx_stream_connect(struct sock *sock, struct sockaddr *uaddr, int addr_len, int flags);
extern int lpx_stream_release(struct sock *sock, struct sock *peer);
extern int lpxitf_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt);

#include <poppack.h>

#endif /* def __NET_LPX_H */
