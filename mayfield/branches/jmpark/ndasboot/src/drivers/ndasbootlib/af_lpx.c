#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include <ntddk.h>

#include "ndasboot.h"
#include "linux2win.h"
#include "errno.h"
#include "time.h"
#include "netdevice.h"
#include "sock.h"
#include "ether.h"
#include "lpx.h"
#include "iov.h"

#define	copy_from_user memcpy
#define	copy_to_user memcpy

//
//	get the current system clock
//	100 Nano-second unit
//
static
__inline
LARGE_INTEGER CurrentTime(
	VOID
	)
{
	LARGE_INTEGER Time;
	ULONG		Tick;
	
	KeQueryTickCount(&Time);
	Tick = KeQueryTimeIncrement();
	Time.QuadPart = Time.QuadPart * Tick;

	return Time;
}

#define	jiffies	(LONGLONG)(CurrentTime().QuadPart)

void lpx_proto_init(void);
void lpx_proto_finito(void);

int 		lpxitf_create(unsigned char *dev_name);
lpx_interface 	*lpxitf_find_using_device(struct device *dev);

void 		lpxitf_insert(lpx_interface *interface);
void 		lpxitf_down(lpx_interface *interface);
int 		lpxitf_demux_sock(lpx_interface *interface, struct sk_buff *skb, int copy);
int 		lpxitf_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt);
int 		lpxitf_device_event(struct notifier_block *notifier, unsigned long event, void *ptr);

struct sock *lpx_create(int type, int protocol);

void 			kfree_message_skb(struct sk_buff *message_skb);

int 		lpx_dgram_create(struct sock *sock, int protocol);
void 		lpx_dgram_rcv(struct sock *sk, struct sk_buff *skb);

int 		lpx_stream_create(struct sock *sock, int protocol);
void		lpx_stream_destroy_sock(struct sock *sk);
void 		lpx_stream_rcv(struct sock *sk, struct sk_buff *skb);

lpx_interface	*lpx_interfaces	= NULL;
struct lpx_portinfo		lpx_port_binder;
struct lpx_eth_cache		lpx_dest_cache;

#define	MAX_ETHER_LEN		10000
int lpx_min_offset = MAX_ETHER_LEN;

//////////////////////////////////////////////////////
// port-binding related function
//////////////////////////////////////////////////////
void				lpx_bind_table_init(void);

void				lpx_dest_cache_init(void);

void				lpx_dest_cache_down(void);

void				lpx_dest_cache_disable(unsigned char * node);

struct sock *	lpx_find_port_bind(unsigned short port);

unsigned short	lpx_get_free_port_num(void);

void				lpx_reg_bind(struct sock * sk);

void				lpx_unreg_bind(struct sock * sk);

unsigned int		lpx_bind_hash(unsigned short port);


		// dest cache function
struct lpx_cache *		lpx_find_dest_cache(struct sock *sk);

void				lpx_update_dest_cache(struct sock * sk);

void lpx_dest_cache_timer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

		// interface address validation function
int				lpx_is_valid_addr(struct sock * sk,unsigned char * node);


#define	SKB_QUEUE(skb)		((struct sk_buff_head *)skb->cb)
#define	LPX_OPT(sk)		((struct lpx_opt *)&sk->protinfo.af_lpx)
#define	LPX_DGRAM_OPT(sk)	((struct lpx_dgram_opt *)&sk->tp_pinfo.af_lpx_dgram)
#define LPX_STREAM_OPT(sk) 		((struct lpx_stream_opt *)&sk->tp_pinfo.af_lpx_stream)

KDPC LpxTimerDpc;

#if 0
int order_count = 0;
char order_list[100];
#endif

void lpx_proto_init(void)
{
	NbDebugPrint(2, ("lpx_proto_init, sizeof(struct lpxhdr) = 0x%x\n", sizeof(struct lpxhdr)));

	lpxitf_create("eth0");
	lpxitf_create("eth1");
	lpxitf_create("eth2");
	lpxitf_create("eth3");

	lpx_bind_table_init();
	lpx_dest_cache_init();

	return;
}


void lpx_proto_finito(void)
{
	lpx_interface	*ifc;

	NbDebugPrint(2, ("lpx_proto_finito\n"));

	while(lpx_interfaces) {
		ifc = lpx_interfaces;
		lpx_interfaces 	= ifc->itf_next;
		ifc->itf_next 	= NULL;
		lpxitf_down(ifc);
	}

	lpx_dest_cache_down();

	return;
}

int lpxitf_create(unsigned char *dev_name)
{
	struct device *dev = &NetDevice;
	lpx_interface 	*interface;

	UNREFERENCED_PARAMETER(dev_name);

	if(dev == NULL)
		return -ENODEV;

	if(dev->addr_len > LPX_NODE_LEN)
		return -EINVAL;

	if(lpxitf_find_using_device(dev) != NULL)
		return -EEXIST;

	if(dev->hard_header_len < lpx_min_offset) {
		lpx_min_offset = dev->hard_header_len;
	}

	interface = (lpx_interface *)kmalloc(sizeof(lpx_interface), 0);
	if(interface == NULL)
		return -EAGAIN;

	interface->itf_dev	= dev;
	interface->itf_sklist 	= NULL;
//	interface->itf_sknum 	= LPX_MIN_EPHEMERAL_SOCKET;
	memcpy(&(interface->itf_node[LPX_NODE_LEN-dev->addr_len]), dev->dev_addr, dev->addr_len);

	lpxitf_insert(interface);

	return 0;
}

lpx_interface *lpxitf_find_using_device(struct device *dev)
{
	lpx_interface	*i;

	NbDebugPrint(2, ("lpxitf_find_using_device\n"));

	for(i = lpx_interfaces; i && (i->itf_dev != dev); i = i->itf_next);

	return i;
}


void lpxitf_insert(lpx_interface *interface)
{
	lpx_interface *i;

	NbDebugPrint(2, ("lpxitf_insert\n"));

	interface->itf_next = NULL;
	if(lpx_interfaces == NULL)
		lpx_interfaces = interface;
	else {
		for(i = lpx_interfaces; i->itf_next != NULL; i = i->itf_next);

		i->itf_next = interface;
	}

	return;
}


void lpxitf_down(lpx_interface *interface)
{
	struct sock *s, *t;

	NbDebugPrint(2, ("lpxitf_down\n"));

	NbDebugPrint(2, ("lpxitf_down interface = %p, interface->itf_sklist = %p\n", interface, interface->itf_sklist));

	lpx_dest_cache_disable(interface->itf_node);

	for(s = interface->itf_sklist; s != NULL; ) {
		s->err = ENOLINK;
		s->error_report(s);
		KeCancelTimer(&LPX_STREAM_OPT(s)->IopTimer);

		LPX_OPT(s)->interface = NULL;
		LPX_OPT(s)->source_addr.port = 0;
		s->zapped = 1;
		t = s;
		s = s->next;
		t->next = NULL;
	}
	interface->itf_sklist = NULL;

	kfree(interface);
	NbDebugPrint(3, ("lpxitf_down end\n"));

	return;
}

lpx_interface *lpxitf_find_using_node(unsigned char *node)
{
	lpx_interface	*i;
	unsigned char	zero_node[LPX_NODE_LEN] = {0, 0, 0, 0, 0, 0};

	for(i = lpx_interfaces; i && memcmp(i->itf_node, node, LPX_NODE_LEN); i = i->itf_next);

	if(i == NULL && memcmp(zero_node, node, LPX_NODE_LEN) == 0)
		i = lpx_interfaces;

	if(i == NULL) {
		NbDebugPrint(0, ("From %02X%02X%02X%02X%02X%02X\n",
                               lpx_interfaces->itf_node[0],
                               lpx_interfaces->itf_node[1],
                               lpx_interfaces->itf_node[2],
                               lpx_interfaces->itf_node[3],
                               lpx_interfaces->itf_node[4],
                               lpx_interfaces->itf_node[5]));

		NbDebugPrint(0, ("From %02X%02X%02X%02X%02X%02X\n",
                               node[0], node[1],
                               node[2], node[3],
                               node[4], node[5]));

	}

	NbDebugPrint(4, ("lpxitf_find_using_node=%p\n", i));

	return i;
}


void lpxitf_insert_sock(lpx_interface *interface, struct sock *sk)
{
	struct sock *s;

	NbDebugPrint(2, ("lpxitf_insert_sock\n"));

	LPX_OPT(sk)->interface = interface;
	sk->next = NULL;
	if(interface->itf_sklist == NULL)
		interface->itf_sklist = sk;
	else {
		for (s = interface->itf_sklist; s->next != NULL; s = s->next);

		s->next = sk;
	}

	sk->zapped = 0;
	
	return;
}


void lpxitf_remove_sock(struct sock *sk)
{
	struct sock 	*s;
	lpx_interface 	*interface;
	KIRQL			oldflags;

//	NbDebugPrint(2, ("lpxitf_remove_socket port = 0x%x\n", NTOHS(((struct lpx_opt *)(sk->protinfo.af_lpx.sk->protinfo.af_unix))->source_addr.port)));

	lpx_unreg_bind(sk);

//	ACQUIRE_SOCKLOCK(sk, oldflags); 
	interface = LPX_OPT(sk)->interface;
	LPX_OPT(sk)->interface = NULL;
	sk->zapped = 1;

	if(interface == NULL) {
//		RELEASE_SOCKLOCK(sk, oldflags);
		NbDebugPrint(1, ("interface is NULL\n"));
		return;
	}

	s = interface->itf_sklist;
	if(s == sk) {
		interface->itf_sklist = s->next;
//		RELEASE_SOCKLOCK(sk, oldflags);
		NbDebugPrint(3, ("removed lpxitf_remove_sock interface = %p, interface->itf_sklist = %p\n", 
				interface, interface->itf_sklist));
		return;
	}

	while(s && s->next) {
		if(s->next == sk) {
			s->next = sk->next;
//			RELEASE_SOCKLOCK(sk, oldflags);
			NbDebugPrint(3, (" lpxitf_remove_sock interface = %p, interface->itf_sklist = %p\n", 
					interface, interface->itf_sklist));
			return;
		}
		s = s->next;
	}
//	RELEASE_SOCKLOCK(sk, oldflags);

	NbDebugPrint(3, ("lpxitf_remove_sock interface = %p, interface->itf_sklist = %p\n", 
				interface, interface->itf_sklist));
}
/*
struct sock *lpxitf_find_sock(lpx_interface *interface, unsigned short port)
{
	struct sock *sk;

	NbDebugPrint(4, ("lpxitf_find_sock port = 0x%x\n", port));

	for(sk = interface->itf_sklist;
		(sk != NULL && LPX_OPT(sk)->source_addr.port != port);
		sk = sk->next);

	return sk;
}
*/


//////////////////////////////////////////////////////
//	port binding and cache manager
//////////////////////////////////////////////////////

// port hash function
unsigned int lpx_bind_hash(unsigned short port_no)
{
	return port_no%PORT_HASH;
}

void lpx_dest_cache_timer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
	
	struct lpx_cache * cache, *p_cache;
	LONGLONG cur_time = jiffies;	
		
	UNREFERENCED_PARAMETER(DeferredContext);

	NbDebugPrint(3,("lpx_dest_cache_timer\n"));

//	KeCancelTimer(&lpx_dest_cache.cache_timer);		
	KeAcquireSpinLockAtDpcLevel(&lpx_dest_cache.cache_lock);			

	cache = lpx_dest_cache.head ;
	p_cache = NULL;	
	
	while(cache)
	{
		if( cur_time - cache->time > CACHE_ALIVE )
		{
			if(NULL == p_cache) {
				lpx_dest_cache.head = cache->next;
				kfree(cache);
				cache = lpx_dest_cache.head;
			}
			else {
				p_cache->next = cache->next;
				kfree(cache);
				cache = p_cache->next;
			}
			continue;				
		}
		p_cache = cache;
		cache = cache->next;
			
	}	
	KeReleaseSpinLockFromDpcLevel(&lpx_dest_cache.cache_lock);

//	lpx_dest_cache.cache_timer.expires = cur_time + CACHE_TIMEOUT;
//	KeSetTimer(&lpx_dest_cache.cache_timer, &lpx_dest_cache.cache_dpc);
}

void	lpx_bind_table_init(void)
{
	int i;

	NbDebugPrint(3, ("lpx_bind_table_init\n"));

	KeInitializeSpinLock(&lpx_port_binder.portalloc_lock);
	lpx_port_binder.last_alloc_port = 0;
	for(i = 0; i < PORT_HASH ; i++)
	{
		KeInitializeSpinLock(&lpx_port_binder.port_hashtable[i].bind_lock);
		lpx_port_binder.port_hashtable[i].bind_next = NULL;
	}
}

void	lpx_dest_cache_init(void)
{
	LARGE_INTEGER deltaTime;

	NbDebugPrint(3, ("lpx_dest_cache_init\n"));

	KeInitializeSpinLock(&lpx_dest_cache.cache_lock);
	lpx_dest_cache.head = NULL;

	KeInitializeDpc( &lpx_dest_cache.cache_dpc, lpx_dest_cache_timer, &lpx_dest_cache );
	KeInitializeTimer(&lpx_dest_cache.cache_timer);	

	deltaTime.QuadPart = - CACHE_TIMEOUT;    /* 2.4 sec. */
//	KeSetTimer(&lpx_dest_cache.cache_timer, &lpx_dest_cache.cache_dpc);
}

void	lpx_dest_cache_down(void)
{
	struct lpx_cache *cache, *p_tmp;
	KIRQL	flags;
	NbDebugPrint(3, ("lpx_dest_cache_down\n"));

	KeAcquireSpinLock(&lpx_dest_cache.cache_lock, &flags);

	cache = lpx_dest_cache.head;
	while(cache)
	{
		p_tmp = cache;
		cache = cache->next;
		kfree(p_tmp);
	}
	lpx_dest_cache.head = NULL;

	KeReleaseSpinLock(&lpx_dest_cache.cache_lock, flags);
	
//	KeCancelTimer(&lpx_dest_cache.cache_timer);
	
}

// check validation of portnumber and seach interface
int	lpx_is_valid_addr(struct sock * sk, unsigned char * node)
{	
	lpx_interface	* i;
	unsigned char zero_node[LPX_NODE_LEN] = {0,0,0,0,0,0};

	NbDebugPrint(3, ("lpx_is_valid_addr\n"));
	for(i = lpx_interfaces; i && memcmp(i->itf_node,node,LPX_NODE_LEN) ; i = i->itf_next);
	
	if(i){
		LPX_OPT(sk)->interface = i;
		memcpy(LPX_OPT(sk)->source_addr.node,i->itf_node, LPX_NODE_LEN);
		return 1;
	}
	else if( i == NULL && (  memcmp(zero_node, node, LPX_NODE_LEN) == 0 )){
		LPX_OPT(sk)->virtual_mapping = 1;
		LPX_OPT(sk)->interface = NULL;
		memcpy(LPX_OPT(sk)->source_addr.node, zero_node, LPX_NODE_LEN);

		NbDebugPrint(0, ("memcmp(zero_node, node, LPX_NODE_LEN) = %d\n", memcmp(zero_node, node, LPX_NODE_LEN)))
		return 1;
	}
	else {
		return 0;
	}

}

// return socket binded with port
struct sock * lpx_find_port_bind( unsigned short port)
{
	struct lpx_bindhead * head;
	struct sock * bind_sk = NULL;
	KIRQL	flags;
	
//	NbDebugPrint(3, ("lpx_find_port_bind=%d\n", port));
	head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];
	
	KeAcquireSpinLock(&head->bind_lock, &flags);
	
	for(bind_sk = head->bind_next; bind_sk; bind_sk = bind_sk->bind_next) {
		if(LPX_OPT(bind_sk)->source_addr.port == port)
			break;
	}

	KeReleaseSpinLock(&head->bind_lock, flags);
	return bind_sk;
}

// return port number is free
unsigned short	lpx_get_free_port_num(void)
{
	unsigned int low = LPX_MIN_EPHEMERAL_SOCKET;
	unsigned int high = LPX_MAX_EPHEMERAL_SOCKET;
	unsigned int remaining = (high - low) +1 ;
	unsigned int port;
	struct lpx_bindhead * head;
	struct sock * bind_sk;
	int ret = 0;
	KIRQL	flags, flags2;

	NbDebugPrint(3, ("lpx_get_free_port_num\n"));
	KeAcquireSpinLock(&lpx_port_binder.portalloc_lock, &flags);
	port = lpx_port_binder.last_alloc_port;
	
	// search availabe port number
	do{
		port++;
		if ((port < low) || (port > high)) port = low;
		head = &lpx_port_binder.port_hashtable[lpx_bind_hash((unsigned short)port)];
		
		KeAcquireSpinLockAtDpcLevel(&head->bind_lock);
		for(bind_sk = head->bind_next; bind_sk; bind_sk = bind_sk->bind_next)
			if(LPX_OPT(bind_sk)->source_addr.port == port)
					goto next;
		break;
next: 
		KeReleaseSpinLockFromDpcLevel(&head->bind_lock);		
	} while ( --remaining > 0);

	KeReleaseSpinLockFromDpcLevel(&head->bind_lock);
	
	if(remaining <= 0) {
		ret = 0;
		goto end;
	}

	lpx_port_binder.last_alloc_port = port;
	ret = port;

end:
	KeReleaseSpinLock(&lpx_port_binder.portalloc_lock, flags);
	return (unsigned short) HTONS(ret);

}



//  insert into bind table
void lpx_reg_bind(struct sock * sk)
{	
	struct lpx_bindhead * head;
	unsigned short port;
	KIRQL	flags, flags2;

	port = LPX_OPT(sk)->source_addr.port;
	NbDebugPrint(3, ("lpx_reg_bind=%d\n", port));

	KeAcquireSpinLock(&lpx_port_binder.portalloc_lock, &flags);	
	head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];
	
	KeAcquireSpinLockAtDpcLevel(&head->bind_lock);
	sk->bind_next = head->bind_next;
	head->bind_next = sk;
	KeReleaseSpinLockFromDpcLevel(&head->bind_lock);

	KeReleaseSpinLock(&lpx_port_binder.portalloc_lock, flags);	

}


void	lpx_unreg_bind(struct sock * sk)
{
	struct lpx_bindhead * head;
	unsigned short port;
	struct sock * s;
	KIRQL	flags, flags2;

	NbDebugPrint(3, ("lpx_unreg_bind\n"));
	port = LPX_OPT(sk)->source_addr.port;
	
	KeAcquireSpinLock(&lpx_port_binder.portalloc_lock, &flags);	
	head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];
	
	KeAcquireSpinLock(&head->bind_lock, &flags2);
	s = head->bind_next;

	if(s == sk)
	{
			head->bind_next = sk->bind_next;
			sk->bind_next = NULL;
			goto end;
	}
	
	while(s && s->bind_next)
	{
		if(s->bind_next == sk){
			s->bind_next = sk->bind_next;
			sk->bind_next = NULL;
			break;
		}
		s = s->bind_next;;
	}

end:	
	KeReleaseSpinLock(&head->bind_lock, flags2);

	KeReleaseSpinLock(&lpx_port_binder.portalloc_lock, flags);	

}


// dest addr cache mechanism
struct lpx_cache *lpx_find_dest_cache(struct sock *sk)
{
	struct lpx_cache * cache;
	KIRQL	flags;

	NbDebugPrint(3, ("lpx_find_dest_cache\n"));
	KeAcquireSpinLock(&lpx_dest_cache.cache_lock, &flags);
	
	for(cache = lpx_dest_cache.head; cache; cache = cache->next) {
		if(!memcmp(cache->dest_addr,LPX_OPT(sk)->dest_addr.node,LPX_NODE_LEN))
		{
			memcpy(LPX_OPT(sk)->source_addr.node,cache->itf_addr,LPX_NODE_LEN);
			break;
		}
	}
	KeReleaseSpinLock(&lpx_dest_cache.cache_lock, flags);

	return cache;
}



//	cache update function
void lpx_update_dest_cache(struct sock * sk)
{
	struct lpx_cache * cache;
	KIRQL	flags;
	
	NbDebugPrint(3, ("lpx_update_dest_cache\n"));
	cache = lpx_find_dest_cache(sk);
	if(!cache){
		cache = kmalloc(sizeof(struct lpx_cache),0);
		if(!cache) {
			NbDebugPrint(0, ("Can't allocate memory\n"));
			return;
		}	

		cache->next = NULL;
		cache->time = jiffies;
//NbDebugPrint(0, ("enter cache entry \n"));
		
		KeAcquireSpinLock(&lpx_dest_cache.cache_lock, &flags);
		cache->next = lpx_dest_cache.head;
		lpx_dest_cache.head = cache;

		KeReleaseSpinLock(&lpx_dest_cache.cache_lock, flags);
	}

	memcpy(cache->dest_addr, LPX_OPT(sk)->dest_addr.node, LPX_NODE_LEN);
	memcpy(cache->itf_addr,LPX_OPT(sk)->source_addr.node,LPX_NODE_LEN);

	return;
}	


void	lpx_dest_cache_disable(unsigned char * node)
{
	struct lpx_cache *cache, *p_cache;
	KIRQL	flags;

//NbDebugPrint(0, ("enter cache entry \n"));
	NbDebugPrint(3, ("lpx_dest_cache_disable\n"));
	KeAcquireSpinLock(&lpx_dest_cache.cache_lock, &flags);
		
	cache = lpx_dest_cache.head;
	p_cache = NULL;
	while(cache)
	{
			if(!memcmp(cache->itf_addr,node,LPX_NODE_LEN))
			{
					if(NULL == p_cache) {
						lpx_dest_cache.head = cache->next;
						kfree(cache);
						cache = lpx_dest_cache.head;
					}
					else {
						p_cache->next = cache->next;
						kfree(cache);
						cache = p_cache->next;
					}
					continue;
			}
			p_cache = cache;
			cache = cache->next;
	}

	KeReleaseSpinLock(&lpx_dest_cache.cache_lock, flags);
	
}

int lpxitf_send(lpx_interface *interface, struct sk_buff *skb, unsigned char *node, int type)
{

	struct device 	*dev = interface->itf_dev;
	char 		dest_node[LPX_NODE_LEN];
	int 		addr_len;
	int		send_to_wire = 1;
	
	NbDebugPrint(4, ("lpxitf_send\n"));

	NbDebugPrint(3, ("From %02X%02X%02X%02X%02X%02X\n",
                               interface->itf_node[0], interface->itf_node[1],
                               interface->itf_node[2], interface->itf_node[3],
                               interface->itf_node[4], interface->itf_node[5]));

	NbDebugPrint(3, ("To   %02X%02X%02X%02X%02X%02X\n",
                               node[0], node[1], node[2], node[3], node[4], node[5]));

	if(memcmp(interface->itf_node, node, LPX_NODE_LEN) == 0) {
		struct sk_buff	*skb2;

		skb->mac.ethernet = (struct ethhdr *)((char *)skb->nh.raw - 14);
		memcpy(skb->mac.ethernet->h_dest, interface->itf_node, LPX_NODE_LEN);
		memcpy(skb->mac.ethernet->h_source, interface->itf_node, LPX_NODE_LEN);
		skb_orphan(skb);
		skb2 = skb_copy(skb, 0);
		kfree_skb(skb);
		if(skb2)
			return lpxitf_demux_sock(interface, skb2, 0);
		return 0;
	}

	/*
	if(memcmp(LPX_BROADCAST_NODE, node, LPX_NODE_LEN) == 0) {
		if(!send_to_wire) {
			struct sk_buff	*skb2;

			skb_orphan(skb);
			skb2 = skb_copy(skb, 0);
			kfree_skb(skb);
			if(skb2)
				lpxitf_demux_sock(interface, skb2, send_to_wire);
			return 0;
		}
		lpxitf_demux_sock(interface, skb, send_to_wire);
	}
	*/

 	if(!send_to_wire) {
		kfree_skb(skb);
		return 0;
	}

	addr_len = dev->addr_len;
	if(memcmp(LPX_BROADCAST_NODE, node, LPX_NODE_LEN) == 0)
		memcpy(dest_node, dev->broadcast, addr_len);
	else
		memcpy(dest_node, &(node[LPX_NODE_LEN-addr_len]), addr_len);

	skb->dev = dev;
	skb->protocol = HTONS(ETH_P_LPX);
	if(dev->hard_header)
		dev->hard_header(skb, dev, ETH_P_LPX, dest_node, NULL, skb->len);

	dev_queue_xmit(skb, type);

	return 0;
}


int lpxitf_rcv(struct sk_buff *skb, struct device *dev, struct packet_type *pt)
{
	lpx_interface	*interface;
	struct lpxhdr	*lpxhdr;

	UNREFERENCED_PARAMETER(pt);

	NbDebugPrint(3, ("lpxitf_rcv\n"));	

#if 0	
	if(order_count < 100) order_list[order_count++] = 'L';	
#endif

	interface = lpxitf_find_using_device(dev);
	if(!interface) {
		NbDebugPrint(1, ("interface is not found\n"));
		kfree_skb(skb);
		return (0);
	}

	lpxhdr = (struct lpxhdr *)skb->nh.raw;

	if((NTOHS(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK)) < sizeof(struct lpxhdr)) {
		NbDebugPrint(1, ("size is small, pktsize = %d, lpxhdr size = %d\n", 
		NTOHS(lpxhdr->pu.pktsize), sizeof(struct lpxhdr)));
		kfree_skb(skb);
		return (0);
	}
		
#if 0
	if(lpx_set_checksum(lpxhdr, NTOHS(lpxhdr->pu.pktsize)) != lpxhdr->checksum){
		NbDebugPrint(0, "lpx_set_checksum error\n");
		kfree_skb(skb);
		return (0);
	}
#endif

	NbDebugPrint(3, ("From %02X%02X%02X%02X%02X%02X:%4X\n",
                               skb->mac.ethernet->h_source[0],
                               skb->mac.ethernet->h_source[1],
                               skb->mac.ethernet->h_source[2],
                               skb->mac.ethernet->h_source[3],
                               skb->mac.ethernet->h_source[4],
                               skb->mac.ethernet->h_source[5],
				NTOHS(lpxhdr->source_port)));

	NbDebugPrint(3, ("To %02X%02X%02X%02X%02X%02X:%4X\n",
                               skb->mac.ethernet->h_dest[0],
                               skb->mac.ethernet->h_dest[1],
                               skb->mac.ethernet->h_dest[2],
                               skb->mac.ethernet->h_dest[3],
                               skb->mac.ethernet->h_dest[4],
                               skb->mac.ethernet->h_dest[5],
				NTOHS(lpxhdr->dest_port)));

	if((memcmp(LPX_BROADCAST_NODE, skb->mac.ethernet->h_dest, LPX_NODE_LEN) == 0)
		|| (memcmp(interface->itf_node, skb->mac.ethernet->h_dest, LPX_NODE_LEN) == 0))
	{
		return (lpxitf_demux_sock(interface, skb, 0));
	}

	kfree_skb(skb);
	return (0);
}


int lpxitf_demux_sock(lpx_interface *interface, struct sk_buff *skb, int copy)
{
	struct lpxhdr *lpxhdr;
	struct sock *sk;


	int is_broadcast;

	NbDebugPrint(4, ("lpxitf_demux_sock\n"));

	skb_queue_head_init(SKB_QUEUE(skb));

//	sk = interface->itf_sklist;
	lpxhdr = (struct lpxhdr *)skb->nh.raw;
	is_broadcast = (memcmp(skb->mac.ethernet->h_dest, LPX_BROADCAST_NODE, LPX_NODE_LEN) == 0);

	sk = lpx_find_port_bind(lpxhdr->dest_port);

	if(sk) {
		NbDebugPrint(4, ("lpxitf_demux_sock: LPX_OPT(sk)->virtual_mapping = %d\n", LPX_OPT(sk)->virtual_mapping));
	}	

//	while(sk != NULL) 
	if(sk)
	{
		if((LPX_OPT(sk)->virtual_mapping)) 
		{
			memcpy(LPX_OPT(sk)->source_addr.node, interface->itf_node, LPX_NODE_LEN);
			LPX_OPT(sk)->virtual_mapping = 0;
			LPX_OPT(sk)->interface = interface;
			lpxitf_insert_sock(interface, sk);
			sk->zapped = 0;
			lpx_update_dest_cache(sk);
		}
		
		if((LPX_OPT(sk)->source_addr.port == lpxhdr->dest_port)
		    && (is_broadcast
			|| (memcmp(skb->mac.ethernet->h_dest, LPX_OPT(sk)->source_addr.node, LPX_NODE_LEN) == 0)))
		{
			struct sk_buff *skb1;			

			if(copy != 0) {
				skb1 = skb_clone(skb, 0);
				if (skb1 == NULL) {
					goto end;
				}
			} else {
				skb1 = skb;
				copy = 1; /* skb may only be used once */
			}
			
			switch(lpxhdr->pu.p.type) 
			{
			case LPX_TYPE_RAW:

				if(sock_queue_rcv_skb(sk, skb) < 0) {
					NbDebugPrint(3, ("sock_queue_rcv_skb error\n"));
					kfree_skb(skb);
				}

				break;
			
			case LPX_TYPE_DATAGRAM:

               	lpx_dgram_rcv(sk,skb);

				break;
			
			case LPX_TYPE_STREAM:

				lpx_stream_rcv(sk,skb);

				break;
			
			default:

				kfree_skb(skb);
				return 0;
			}

		}
//		sk = sk->next;
	}
end:
	if(copy == 0)
		kfree_skb(skb);

	return (0);
}


int lpxitf_offset(struct sock *sk) 
{
	if(LPX_OPT(sk)->virtual_mapping) return lpx_min_offset;
	return LPX_OPT(sk)->interface->itf_dev->hard_header_len;
}


int lpxitf_mtu(struct sock *sk)
{
        struct device *dev;

        dev = LPX_OPT(sk)->interface->itf_dev;

        if(dev == NULL)
                return -ENXIO;

        return dev->mtu;
}


void kfree_message_skb(struct sk_buff *message_skb)
{
	struct sk_buff	*fragment_skb;

	while((fragment_skb = skb_dequeue(SKB_QUEUE(message_skb))) != NULL)
	{
		kfree_skb(fragment_skb);
	}

	kfree_skb(message_skb);
}


struct sock *lpx_create(int type, int protocol)
{
	struct sock *sock;
	int status = 0;

	NbDebugPrint(2, ("lpx_create: type = %d, protocol = %d\n", type, protocol));

//	if(sock->type != SOCK_DGRAM
//		&& sock->type != SOCK_SEQPACKET
//		&& sock->type != SOCK_STREAM)
	if(type!= SOCK_DGRAM
		&& type != SOCK_SEQPACKET
		&& type != SOCK_STREAM)
	{
//		return -ESOCKTNOSUPPORT;
		return NULL;
	}

	sock = sk_alloc(PF_LPX, 0, 1);
	if(sock == NULL) {		
//		return -ENOMEM;
		return NULL;
	}

//	sock_init_data(sock, sk);
	NbDebugPrint(2, ("lpx_create: sock_init_data\n"));
	sock_init_data(sock, &NetDevice, (unsigned short)type);
//	sock->destruct	= NULL;

//	if(sock->type ==  SOCK_DGRAM) 
	if(type ==  SOCK_DGRAM) 
	{
		status =  lpx_dgram_create(sock, protocol);
		
//	} else if(sock->type ==  SOCK_SEQPACKET
//		|| sock->type ==  SOCK_STREAM)
	} else if(type ==  SOCK_SEQPACKET
		|| type ==  SOCK_STREAM)
	{
		status = lpx_stream_create(sock, protocol);
	}

	return sock;
}


int 		lpx_dgram_release(struct sock *sock, struct sock *peer);
int 		lpx_dgram_bind(struct sock *sock, struct sockaddr *uaddr, int addr_len);
//unsigned short	lpx_first_free_socketnum(lpx_interface *interface);
int 		lpx_dgram_connect(struct sock *sock, struct sockaddr *uaddr, int addr_len, int flags);
int 		lpx_dgram_getname(struct sock *sock, struct sockaddr *uaddr, int *uaddr_len, int peer);
//__u16 		lpx_set_checksum(struct lpxhdr *packet,int length);
int 		lpx_dgram_ioctl(struct sock *sock, unsigned int cmd, unsigned long arg);
int 		lpx_dgram_sendmsg(struct sock *sock, struct msghdr *msg, int len, struct scm_cookie *scm);
int 		lpx_dgram_recvmsg(struct sock *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm);
int 		lpx_transmit_packet(struct sock *sk, struct sockaddr_lpx *usaddr, struct iovec *iov, int len, int noblock);

int lpx_dgram_create(struct sock *sock, int protocol)
{
	struct sock *sk;

	UNREFERENCED_PARAMETER(protocol); 

 	NbDebugPrint(2, ( "called lpx_dgram_create\n"));

//	sock->ops = &lpx_dgram_ops;

	sk = sock;

	LPX_DGRAM_OPT(sk)->message_id = (unsigned short) jiffies;
	skb_queue_head_init(&LPX_DGRAM_OPT(sock)->receive_packet_queue);
	LPX_DGRAM_OPT(sk)->receive_data_size = 0;

//	sk->sndbuf	*= 32;
//	sk->rcvbuf	*= 32;

	return 0;
}


/* Create the SMP specific data */
int lpx_dgram_release(struct sock *sock, struct sock *peer)
{
//	struct sock 	*sk = sock->sk;
	struct sock 	*sk = sock;
	struct sk_buff	*skb;

	UNREFERENCED_PARAMETER(peer);

	NbDebugPrint(2, ("lpx_dgram_release\n"));

	if(sk == NULL)
		return 0;

	if(!sk->dead)
		sk->state_change(sk);

	sk->dead = 1;
//	sock->sk = NULL;

	lpxitf_remove_sock(sk);

	while((skb = skb_dequeue(&LPX_DGRAM_OPT(sk)->receive_packet_queue)) != NULL)
		kfree_message_skb(skb);

	while((skb = skb_dequeue(&sk->receive_queue)) != NULL)
		kfree_skb(skb);

	sk_free(sk);	

	return 0;
}


int lpx_dgram_bind(struct sock *sock, struct sockaddr *uaddr, int addr_len)
{
//	struct sock 		*sk = sock->sk;
	struct sock 		*sk = sock;
//	lpx_interface 		*interface;
	struct sockaddr_lpx 	*addr = (struct sockaddr_lpx *)uaddr;
	int valid = 0;

	NbDebugPrint(0, ("lpx_dgram_bind, zapped = %d, addr_len = %d, sizeof(struct sockaddr_lpx) = %d\n", sk->zapped, addr_len, sizeof(struct sockaddr_lpx)));

	if(sk->zapped == 0)
		return -EINVAL;

	if(addr_len != sizeof(struct sockaddr_lpx))
		return -EINVAL;

	NbDebugPrint(0, ("From %02X%02X%02X%02X%02X%02X\n",
                               addr->slpx_node[0], addr->slpx_node[1],
                               addr->slpx_node[2], addr->slpx_node[3],
                               addr->slpx_node[4], addr->slpx_node[5]));

	// step 1 : validation of address
	//	addr must be either valid ethnet addr or zero addr
	
	valid = lpx_is_valid_addr(sk, addr->slpx_node);
	NbDebugPrint(0, ("valid = %d\n", valid));
	if(!valid)
		return -EADDRNOTAVAIL;

	NbDebugPrint(0, ("From %02X%02X%02X%02X%02X%02X\n",
                               addr->slpx_node[0], addr->slpx_node[1],
                               addr->slpx_node[2], addr->slpx_node[3],
                               addr->slpx_node[4], addr->slpx_node[5]));

	// step 2 : get port number
	if(addr->slpx_port == 0) {
		addr->slpx_port = lpx_get_free_port_num();
		if(addr->slpx_port == 0)
			return -EINVAL;
	}

	
	/* protect LPX system stuff like routing/sap */

	NbDebugPrint(0, ("addr->slpx_port = %04X, LPX_MIN_EPHEMERAL_SOCKET = %04X, LPX_MAX_EPHEMERAL_SOCKET = %04X\n", 
			NTOHS(addr->slpx_port), 
			LPX_MIN_EPHEMERAL_SOCKET, 
			LPX_MAX_EPHEMERAL_SOCKET));

//	if(NTOHS(addr->slpx_port) < LPX_MIN_EPHEMERAL_SOCKET && !capable(CAP_NET_ADMIN))
	if(NTOHS(addr->slpx_port) < LPX_MIN_EPHEMERAL_SOCKET)
		return -EACCES;

	// step 3 : check port number validation
	if(lpx_find_port_bind(addr->slpx_port) != NULL)
		return -EADDRINUSE;
	

	// step 4 : register sock to binder
	LPX_OPT(sk)->source_addr.port = addr->slpx_port;
	lpx_reg_bind(sk);
	if(LPX_OPT(sk)->interface) lpxitf_insert_sock(LPX_OPT(sk)->interface, sk);
	
	NbDebugPrint(0, ("lpx_dgram_bind: LPX_OPT(sk)->interface = %p, sk->zapped = %d\n", LPX_OPT(sk)->interface, sk->zapped));

/*	interface = lpxitf_find_using_node(addr->slpx_node);
	if(interface == NULL)
		return -EADDRNOTAVAIL;

	NbDebugPrint(3, ("From %02X%02X%02X%02X%02X%02X\n",
                               addr->slpx_node[0], addr->slpx_node[1],
                               addr->slpx_node[2], addr->slpx_node[3],
                               addr->slpx_node[4], addr->slpx_node[5]));

	if(addr->slpx_port == 0) {
		addr->slpx_port = lpx_first_free_socketnum(interface);
		if(addr->slpx_port == 0)
			return -EINVAL;
	}
*/
	/* protect LPX system stuff like routing/sap */
/*	if(NTOHS(addr->slpx_port) < LPX_MIN_EPHEMERAL_SOCKET && !capable(CAP_NET_ADMIN))
		return -EACCES;

	if(lpxitf_find_sock(interface, addr->slpx_port) != NULL)
		return -EADDRINUSE;

	LPX_OPT(sk)->source_addr.port = addr->slpx_port;
	memcpy(LPX_OPT(sk)->source_addr.node, interface->itf_node, LPX_NODE_LEN);

	lpxitf_insert_sock(interface, sk);
*/
	return 0;
}

/*
unsigned short lpx_first_free_socketnum(lpx_interface *interface)
{
	unsigned short socketnum = interface->itf_sknum;

	NbDebugPrint(3, ("lpx_firtst_free_socketnum\n"));

	if(socketnum < LPX_MIN_EPHEMERAL_SOCKET)
		socketnum = LPX_MIN_EPHEMERAL_SOCKET;

	while(lpxitf_find_sock(interface, NTOHS(socketnum)) != NULL) {
		if(socketnum > LPX_MAX_EPHEMERAL_SOCKET)
			socketnum = LPX_MIN_EPHEMERAL_SOCKET;
		else
			socketnum++;
	}

	interface->itf_sknum = socketnum;

	return NTOHS(socketnum);
}
*/

int lpx_dgram_connect(struct sock *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
//	struct sock *sk = sock->sk;
	struct sock *sk = sock;
	struct sockaddr_lpx *addr;

	UNREFERENCED_PARAMETER(flags);

	NbDebugPrint(1, ("lpx_dgram_connect\n"));	

//	sk->state	= TCP_CLOSE;
//	sock->state 	= SS_UNCONNECTED;
	if(addr_len != sizeof(struct sockaddr_lpx))
		return -EINVAL;

	addr = (struct sockaddr_lpx *)uaddr;
	if(LPX_OPT(sk)->source_addr.port == 0) {
		struct sockaddr_lpx 	uaddr;
		int 			ret;
		unsigned char zero_node[LPX_NODE_LEN] = {0,0,0,0,0,0};

		uaddr.slpx_port	= 0;

		if(LPX_OPT(sk)->interface)
			memcpy(uaddr.slpx_node, LPX_OPT(sk)->interface->itf_node,LPX_NODE_LEN);
		else
			memcpy(uaddr.slpx_node, zero_node,LPX_NODE_LEN);

		ret = lpx_dgram_bind(sock, (struct sockaddr *)&uaddr,
				sizeof(struct sockaddr_lpx));
		if(ret != 0)
			return ret;
	}
	
	LPX_OPT(sk)->dest_addr.port = addr->slpx_port;
	memcpy(LPX_OPT(sk)->dest_addr.node, addr->slpx_node,LPX_NODE_LEN);

	if(LPX_OPT(sk)->virtual_mapping) {
		struct lpx_cache *ret;

		ret = lpx_find_dest_cache(sk);
		if(ret) {
			LPX_OPT(sk)->interface = lpxitf_find_using_node(LPX_OPT(sk)->source_addr.node);
			LPX_OPT(sk)->virtual_mapping = 0;
			lpxitf_insert_sock(LPX_OPT(sk)->interface, sk);
		}
	}
	else {
		lpx_update_dest_cache(sk);
	}

//	if(sock->type == SOCK_DGRAM) {
	if(sk->type == SOCK_DGRAM) {
//		sock->state 	= SS_CONNECTED;
//		sk->state 	= TCP_ESTABLISHED;
	}

	return 0;
}


int lpx_dgram_getname(struct sock *sock, struct sockaddr *uaddr, int *uaddr_len, int peer)
{
        lpx_address *addr;
        struct sockaddr_lpx saddr;
        struct sock *sk;

	NbDebugPrint(3, ("lpx_dgram_getname\n"));

//        sk = sock->sk;
	sk = sock;

        *uaddr_len = sizeof(struct sockaddr_lpx);

        if(peer) {
//                if(sk->state != TCP_ESTABLISHED)
//                        return (-ENOTCONN);

                addr = &LPX_OPT(sk)->dest_addr;
                memcpy(saddr.slpx_node, addr->node, LPX_NODE_LEN);
                saddr.slpx_port = addr->port;
        } else {
                if(LPX_OPT(sk)->interface != NULL) {
                        memcpy(saddr.slpx_node, LPX_OPT(sk)->interface->itf_node, LPX_NODE_LEN);
                } else {
                        memset(saddr.slpx_node, '\0', LPX_NODE_LEN);
                }

                saddr.slpx_port = LPX_OPT(sk)->source_addr.port;
        }

        saddr.slpx_family = AF_LPX;
        memcpy(uaddr, &saddr, sizeof(struct sockaddr_lpx));

        return (0);
}

int lpx_dgram_ioctl(struct sock *sock, unsigned int cmd, unsigned long arg)
{
//	struct sock *sk = sock->sk;
	struct sock *sk = sock;

	UNREFERENCED_PARAMETER(arg);

	switch(cmd) {
		case IOCTL_DEL_TIMER:
			KeCancelTimer(&LPX_STREAM_OPT(sk)->IopTimer);
			break;
		default:
			break;
	}

	return 0;
}

int lpx_dgram_sendmsg(struct sock *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
{
//	struct sock 		*sk = sock->sk;
	struct sock 		*sk = sock;
	struct sockaddr_lpx 	*usaddr = (struct sockaddr_lpx *)msg->msg_name;
	struct sockaddr_lpx 	local_saddr;
	int 			retval;
	int 			flags = msg->msg_flags;

	
	UNREFERENCED_PARAMETER(scm);

	NbDebugPrint(3, ("lpx_dgram_sendmsg\n"));

	if(flags & ~MSG_DONTWAIT)
		return -EINVAL;

	if(usaddr) {
		if(LPX_OPT(sk)->source_addr.port == 0) {
			struct sockaddr_lpx saddr;

			saddr.slpx_port = 0;
			if(LPX_OPT(sk)->interface)
				memcpy(saddr.slpx_node, LPX_OPT(sk)->interface->itf_node,LPX_NODE_LEN);
			else
				return -ENETDOWN;

			retval = lpx_dgram_bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_lpx));
			if(retval != 0)
				return retval;
		}

		if(msg->msg_namelen < sizeof(struct sockaddr_lpx))
			return -EINVAL;

		if(usaddr->slpx_family != AF_LPX)
			return -EINVAL;

		if(LPX_OPT(sk)->virtual_mapping) {
			struct lpx_cache *ret;

			memcpy(LPX_OPT(sk)->dest_addr.node, usaddr->slpx_node, LPX_NODE_LEN);
			LPX_OPT(sk)->dest_addr.port = usaddr->slpx_port;
			ret = lpx_find_dest_cache(sk);
			if(ret) {
				LPX_OPT(sk)->interface = lpxitf_find_using_node(LPX_OPT(sk)->source_addr.node);
				LPX_OPT(sk)->virtual_mapping = 0;
				lpxitf_insert_sock(LPX_OPT(sk)->interface, sk);
			}
		}
		else {
			lpx_update_dest_cache(sk);
		}
	} else {
//		if(sk->state != TCP_ESTABLISHED)
//			return -ENOTCONN;

		usaddr = &local_saddr;
		usaddr->slpx_family = AF_LPX;
		usaddr->slpx_port = LPX_OPT(sk)->dest_addr.port;
		memcpy(usaddr->slpx_node, LPX_OPT(sk)->dest_addr.node,LPX_NODE_LEN);
	}

	retval = lpx_transmit_packet(sk, usaddr, msg->msg_iov, len, flags&MSG_DONTWAIT);
	if(retval < 0)
		return retval;

	return len;
}


int lpx_dgram_recvmsg(struct sock *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
{
//	struct sock 		*sk = sock->sk;
	struct sock 		*sk = sock;
	struct sockaddr_lpx 	*saddr = (struct sockaddr_lpx *)msg->msg_name;
	struct lpxhdr 		*lpxhdr = NULL;
	struct sk_buff 		*skb;
	int 			copied, err;
	int			user_data_length;
	int			total_size;
//	struct sk_buff		*fragment_skb;

	UNREFERENCED_PARAMETER(scm);
	
	NbDebugPrint(4, ("lpx_dgram_recvmsg\n"));	

	if(LPX_OPT(sk)->source_addr.port == 0) {
		struct sockaddr_lpx 	uaddr;
		int 			ret;

		uaddr.slpx_port = 0;

		if(LPX_OPT(sk)->interface)
			memcpy(uaddr.slpx_node, LPX_OPT(sk)->interface->itf_node,LPX_NODE_LEN);
		else
			return -ENETDOWN;

		ret = lpx_dgram_bind(sock, (struct sockaddr *)&uaddr, sizeof(struct sockaddr_lpx));
		if(ret != 0)
			return ret;
	}
	
	if(sk->zapped != 0)
		return -ENOTCONN;

	skb = skb_recv_datagram(sk ,flags&~MSG_DONTWAIT, flags&MSG_DONTWAIT, &err);
	if(!skb)
		goto out;

	total_size = 0;

	lpxhdr = (struct lpxhdr *)skb->nh.raw;

	user_data_length = NTOHS(lpxhdr->u.d.message_length);
	if(user_data_length > size) {
		user_data_length = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	copied = (NTOHS(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK))- sizeof(struct lpxhdr);

	NbDebugPrint(3, ("lpx_dgram_recvmsg user_data_length = %d, copied = %d\n", user_data_length, copied));
	if((total_size + copied) > user_data_length)
	{
		copied = user_data_length - total_size;
	}

	total_size += copied;

	err = skb_copy_datagram_iovec(skb, sizeof(struct lpxhdr), msg->msg_iov, copied);
	NbDebugPrint(3, ("lpx_dgram_recvmsg err = %d\n", err));
/*
	fragment_skb = skb_peek(SKB_QUEUE(skb));
	NbDebugPrint(3, ("lpx_dgram_recvmsg fragment_skb = %p, NULL = %p\n", fragment_skb, NULL));

#if 1
	
	while((fragment_skb = skb_dequeue(SKB_QUEUE(skb))) != NULL)
	{
		struct lpxhdr	*fragment_lpxhdr = NULL;

		fragment_lpxhdr = (struct lpxhdr *)fragment_skb->nh.raw;

		copied = (NTOHS(fragment_lpxhdr->pu.pktsize & ~LPX_TYPE_MASK))- sizeof(struct lpxhdr);

		if((total_size + copied) > user_data_length)
		{
			copied = user_data_length - total_size;
		}

		total_size += copied;
		
		if(copied && err == 0)
			err = skb_copy_datagram_iovec(fragment_skb, sizeof(struct lpxhdr), msg->msg_iov, copied);
		kfree_skb(fragment_skb);
	}

#endif
*/
	if(err)
		goto out_free;

//	sk->stamp = skb->stamp;

	msg->msg_namelen = sizeof(struct sockaddr_lpx);

	if(saddr) {
		saddr->slpx_family 	= AF_LPX;
		saddr->slpx_port 	= lpxhdr->source_port;
		memcpy(saddr->slpx_node, skb->mac.ethernet->h_source, LPX_NODE_LEN);
	}

	err = user_data_length;

out_free:
	skb_free_datagram(sk, skb);
out:
	return err;
}


int lpx_transmit_packet(struct sock *sk, struct sockaddr_lpx *usaddr, struct iovec *iov, int len, int noblock) {
	struct sk_buff  *skb;
	lpx_interface   *interface;
	struct lpxhdr   *lpxhdr;
	int	     	size;
	int	     	lpx_offset;
	int	     	err;

	int		dev_mtu;
	int		mss_now;
	unsigned short	fragment_id;
	unsigned short	copy;
	unsigned short	user_data_len;


	NbDebugPrint(4, ("lpx_transmit_packet\n"));

	user_data_len = (unsigned short)len;

	interface = LPX_OPT(sk)->interface;
	lpx_offset = interface->itf_dev->hard_header_len;

	dev_mtu = lpxitf_mtu(sk);
	mss_now = dev_mtu - sizeof(struct lpxhdr);
	
	fragment_id = 0;
	LPX_DGRAM_OPT(sk)->message_id ++;

	while(user_data_len) {
		copy = (unsigned short)mss_now;
		
		if(copy > user_data_len)
			copy = user_data_len;

		user_data_len -= copy;

		size = lpx_offset + sizeof(struct lpxhdr) + copy;

		skb = sock_alloc_send_skb(sk, size, 0, noblock, &err);
		if(!skb)
			return err;

		skb_reserve(skb, lpx_offset);
		skb->sk = sk;
		lpxhdr = (struct lpxhdr *)skb_put(skb, sizeof(struct lpxhdr));
		lpxhdr->pu.pktsize = HTONS(copy + sizeof(struct lpxhdr));
		lpxhdr->pu.p.type = LPX_TYPE_DATAGRAM;
		lpxhdr->source_port = LPX_OPT(sk)->source_addr.port;
		lpxhdr->dest_port = usaddr->slpx_port;

		lpxhdr->u.d.message_id = LPX_DGRAM_OPT(sk)->message_id;
		lpxhdr->u.d.message_length = HTONS(len);
		lpxhdr->u.d.fragment_id = HTONS(fragment_id);
		fragment_id ++;
/*
		if(user_data_len == 0)
			lpxhdr->u.d.fragment_last = 1;
		else
			lpxhdr->u.d.fragment_last = 0;
*/
	
		skb->h.raw = skb->nh.raw = (void *) lpxhdr;

		err = memcpy_fromiovec(skb_put(skb, copy), iov, copy);
		if(err) {
			kfree_skb(skb);
			return -EFAULT;
		}

		if(!interface) {
			lpx_interface *p_if;
			for(p_if = lpx_interfaces; p_if; p_if = p_if->itf_next) {
				struct sk_buff *skb2;
				skb2 = skb_clone(skb, 0);
				if(skb2) {
					lpxitf_send(p_if, skb, usaddr->slpx_node, DATA);
				}
			}
			kfree(skb);
		}
		else {
			lpxitf_send(interface, skb, usaddr->slpx_node, DATA);
		}
	}
	
	NbDebugPrint(4, ("lpx_transmit_packet End\n"));

	return 0;
}


void lpx_dgram_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct lpxhdr *lpxhdr;
	
	NbDebugPrint(3, ("lpx_dgram_rcv\n"));

#if 0
	if(order_count < 100) order_list[order_count++] = 'D';	
#endif

	lpxhdr = (struct lpxhdr *)skb->nh.raw;

	if(NTOHS(lpxhdr->u.d.fragment_id) == 0)
	{
		struct sk_buff_head	temp_queue;
		struct sk_buff		*temp_skb;

		skb_queue_head_init(&temp_queue);

		while((temp_skb = skb_dequeue(&LPX_DGRAM_OPT(sk)->receive_packet_queue)) != NULL)
		{
			struct lpxhdr *temp_lpxhdr;
	
			temp_lpxhdr = (struct lpxhdr *)temp_skb->nh.raw;

			if(lpxhdr->u.d.message_id == temp_lpxhdr->u.d.message_id
				&& (!memcmp(skb->mac.ethernet->h_source, temp_skb->mac.ethernet->h_source, 6)
					&& lpxhdr->source_port == temp_lpxhdr->source_port))
			{
				LPX_DGRAM_OPT(sk)->receive_data_size -= NTOHS(temp_lpxhdr->u.d.message_length);
				kfree_message_skb(temp_skb);
			} else
			{
				skb_queue_head(&temp_queue, temp_skb);
			}
			
		}

		while((temp_skb = skb_dequeue(&temp_queue)) != NULL)
		{
			skb_queue_head(&LPX_DGRAM_OPT(sk)->receive_packet_queue, temp_skb);
		}
			
//		if(lpxhdr->u.d.fragment_last) {
			if(sock_queue_rcv_skb(sk, skb) < 0) {
				NbDebugPrint(3, ("sock_queue_rcv_skb error\n"));
				kfree_skb(skb);
			}

			return;
/*		} else
		{
			while((LPX_DGRAM_OPT(sk)->receive_data_size + NTOHS(lpxhdr->u.d.message_length)) 
				> MAX_RECEIVE_DATA_SIZE)
			{
				struct lpxhdr *temp_lpxhdr;

				temp_skb = skb_dequeue(&LPX_DGRAM_OPT(sk)->receive_packet_queue);
				if(temp_skb == NULL) {
					NbDebugPrint(0, "lpx not statble\n");
					break;
				}

	
				temp_lpxhdr = (struct lpxhdr *)temp_skb->nh.raw;

				LPX_DGRAM_OPT(sk)->receive_data_size -= NTOHS(temp_lpxhdr->u.d.message_length);
				kfree_message_skb(temp_skb);
			}
	
			LPX_DGRAM_OPT(sk)->receive_data_size += NTOHS(lpxhdr->u.d.message_length);
			skb_queue_head(&LPX_DGRAM_OPT(sk)->receive_packet_queue, skb);
			
			return;
		} 
*/
	}
/*
	else
	{
		struct sk_buff_head	temp_queue;
		struct sk_buff		*message_skb;
		struct sk_buff		*temp_skb;


		skb_queue_head_init(&temp_queue);

		while((message_skb = skb_dequeue(&LPX_DGRAM_OPT(sk)->receive_packet_queue)) != NULL)
		{
			struct lpxhdr *message_lpxhdr;
	
			skb_queue_head(&temp_queue, message_skb);

			message_lpxhdr = (struct lpxhdr *)message_skb->nh.raw;

			if(lpxhdr->u.d.message_id == message_lpxhdr->u.d.message_id
				&& (!memcmp(skb->mac.ethernet->h_source, message_skb->mac.ethernet->h_source, 6)
					&& lpxhdr->source_port == message_lpxhdr->source_port))
			{
				skb_queue_tail(SKB_QUEUE(message_skb), skb);
				
				break;
			}
			
		}

		while((temp_skb = skb_dequeue(&temp_queue)) != NULL)
		{
			skb_queue_head(&LPX_DGRAM_OPT(sk)->receive_packet_queue, temp_skb);
		}
			
		if(message_skb == NULL) {
			NbDebugPrint(2, ("no meessage_skb\n"));
			kfree_skb(skb);
			return;
		}
			
//		if(lpxhdr->u.d.fragment_last) {
			NbDebugPrint(4, ("meessage_skb\n"));
			skb_unlink(message_skb);
//			kfree_message_skb(message_skb);
//			kfree_skb(skb);
//			return;
			if(sock_queue_rcv_skb(sk, message_skb) < 0) {
				NbDebugPrint(3, ("sock_queue_rcv_skb error\n"));
				kfree_message_skb(message_skb);
			}

			return;
//		} 
	}
*/
}


#if 0

__u16 lpx_set_checksum(struct lpxhdr *packet,int length)
 {
	__u32 sum = 0;
	__u16 *p = (__u16 *)&packet->pktsize;
	__u32 i = length >> 1;

	while(--i)
		sum += *p++;

	if(packet->pktsize & HTONS(1))
		sum += NTOHS(0xff00) & *p;

	sum = (sum & 0xffff) + (sum >> 16);

	/* It's a pity there's no concept of carry in C */
	if(sum >= 0x10000)
		sum++;

	return ~sum;
}

#endif

// #include <asm/uaccess.h>

#define SMP_DEBUG	1

/* Functions needed for SMP connection start up */

int lpx_stream_release(struct sock *sock, struct sock *peer);
int  lpx_stream_bind(struct sock *sock, struct sockaddr *uaddr, int addr_len);
int lpx_stream_connect(struct sock *sock, struct sockaddr *uaddr, int addr_len, int flags);
int lpx_stream_accept(struct sock *sock, struct sock *newsock, int flags);
int lpx_stream_getname (struct sock *sock, struct sockaddr *uaddr, int *usockaddr_len, int peer);
int lpx_stream_ioctl(struct sock *sock, unsigned int cmd, unsigned long arg);
int lpx_stream_listen(struct sock *sock, int backlog);
int lpx_stream_sendmsg(struct sock *sock, struct msghdr *msg, int len);
int lpx_stream_recvmsg(struct sock *sock, struct msghdr *msg, int size, int flags, PLARGE_INTEGER	TimeOut);

int lpx_stream_opt_init(struct sock *sk);

int lpx_stream_do_sendmsg(struct sock *sk, struct msghdr *msg);
void wait_for_lpx_stream_memory(struct sock * sk);
__inline int lpx_stream_memory_free(struct sock *sk);
int  lpx_stream_transmit(struct sock *sk, struct sk_buff *skb, int type,int len);
int lpx_stream_route_skb(struct sock *sk, struct sk_buff *skb, int type);
__inline unsigned long lpx_stream_calc_rtt(struct sock *sk);
int lpx_stream_snd_test(struct sock *sk, struct sk_buff *skb);

int lpx_stream_retransmit_chk(struct sock *sk, unsigned short ackseq, int type);
void lpx_stream_rcv(struct sock *sk, struct sk_buff *skb);
int  lpx_stream_do_rcv(struct sock* sk, struct sk_buff *skb);
//void flight_adaption (struct sock *sk, unsigned short success_packets);

void lpx_stream_timer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

int lpx_stream_create(struct sock *sock, int protocol)
{
	struct sock *sk;

	UNREFERENCED_PARAMETER(protocol);
 	NbDebugPrint(2, ("called lpx_stream_create\n"));

//	sock->ops 	= &lpx_stream_operations;

//	sk = sock->sk;
	sk = sock;

//	sk->sndbuf	*= 32;
//	sk->rcvbuf	*= 32;
//	sk->sndbuf	*= 64;
//	sk->rcvbuf	*= 64;
	sk->backlog_rcv = lpx_stream_do_rcv;

	lpx_stream_opt_init(sk);
	
	return (0);
}


/* Create the SMP specific data */
int lpx_stream_opt_init(struct sock *sk)
{
	memset(LPX_STREAM_OPT(sk), 0, sizeof(struct lpx_stream_opt));

	LPX_STREAM_OPT(sk)->owner		= (void *)sk;

	LPX_STREAM_OPT(sk)->alloc		= SMP_MAX_FLIGHT;
	LPX_STREAM_OPT(sk)->rmt_alloc	= SMP_MAX_FLIGHT;
	LPX_STREAM_OPT(sk)->interval_time	= HZ;

    KeInitializeDpc( &LpxTimerDpc, lpx_stream_timer, sk );
    KeInitializeTimerEx( &LPX_STREAM_OPT(sk)->IopTimer, NotificationTimer );
	LPX_STREAM_OPT(sk)->deltaTime.QuadPart = - SMP_TIMEOUT;    // 100 ns interval
//	DbgPrint("LPX_STREAM_OPT(sk)->deltaTime.QuadPart = %I64X, jiffies = %I64X, SMP_TIMEOUT = %I64X\n", 
//		LPX_STREAM_OPT(sk)->deltaTime.QuadPart, 
//		jiffies, 
//		SMP_TIMEOUT);

	
    KeSetTimer( &LPX_STREAM_OPT(sk)->IopTimer, LPX_STREAM_OPT(sk)->deltaTime, &LpxTimerDpc );

//	init_timer(&LPX_STREAM_OPT(sk)->lpx_stream_timer);
//	LPX_STREAM_OPT(sk)->lpx_stream_timer.function 	= &lpx_stream_timer;
//	LPX_STREAM_OPT(sk)->lpx_stream_timer.data		= (unsigned long)sk;

	skb_queue_head_init(&LPX_STREAM_OPT(sk)->retransmit_queue);

	LPX_STREAM_OPT(sk)->max_flights = SMP_MAX_FLIGHT / 2;

	return 0;
}


/* Release an SMP socket */
int lpx_stream_release(struct sock *sock, struct sock *peer)
{
// 	struct sock 	*sk = sock->sk;
 	struct sock 	*sk = sock;
	int 		err = 0;

	UNREFERENCED_PARAMETER(peer);

	NbDebugPrint(0, ("lpx_stream_release sk->state = %d\n", sk->state));
	if(sk == NULL)
		return 0;

	lock_sock(sk);
	sk->shutdown	= SHUTDOWN_MASK;

	switch(sk->state) {

	case SMP_CLOSE:
	case SMP_LISTEN:
	case SMP_SYN_SENT:

		sk->state = SMP_CLOSE;
		if(!sk->dead)
			sk->state_change(sk);
		sk->dead = 1;

		break;

	case SMP_SYN_RECV:
	case SMP_ESTABLISHED:
	case SMP_CLOSE_WAIT:

		if(sk->state == SMP_CLOSE_WAIT)
			sk->state = SMP_LAST_ACK;
		else		
			sk->state = SMP_FIN_WAIT1; 

		if(!sk->dead)
			sk->state_change(sk);
		sk->dead = 1;

		
		LPX_STREAM_OPT(sk)->fin_seq = LPX_STREAM_OPT(sk)->sequence;
		lpx_stream_transmit(sk, NULL, DISCON, 0);

		break;

	default:
		NbDebugPrint(0, ("It's unstable state\n"));
	}

//	sk->sleep = NULL;

//	MpDebugPrint((0, "lpx_stream_release: release_sock = %d\n", atomic_read(&sk->sock_readers)));
	release_sock(sk);

#ifdef __NDASBOOT__
	if(sk->destroy) lpx_stream_destroy_sock(sk);
#endif

	return err;
}

int destroy_count = 0;

void lpx_stream_destroy_sock(struct sock *sk)
{
	struct sk_buff *skb;	
	KIRQL	flags;
	int back_log = 0, receive_queue = 0, write_queue = 0, retransmit_queue = 0;
	
	destroy_count ++;
	NbDebugPrint(0, ("%dth lpx_stream_destroy_sock %p\n",destroy_count,sk));
//	if(destroy_count >= 2) return;

//	NbDebugPrint(2, ("sk->wmem_alloc = %x, sk->rmem_alloc = %x\n", 
//			atomic_read(&sk->wmem_alloc), atomic_read(&sk->rmem_alloc)));
	NbDebugPrint(2, ("sequence = %x, fin_seq = %x, rmt_seq = %x, rmt_ack = %x\n", 
		LPX_STREAM_OPT(sk)->sequence, LPX_STREAM_OPT(sk)->fin_seq, LPX_STREAM_OPT(sk)->rmt_seq, LPX_STREAM_OPT(sk)->rmt_ack));
	NbDebugPrint(2, ("last_retransmit_seq=%x, reason = %x\n", 
		LPX_STREAM_OPT(sk)->last_retransmit_seq, LPX_STREAM_OPT(sk)->timer_reason));

	ACQUIRE_SOCKLOCK(sk,flags);		

	if(LPX_STREAM_OPT(sk)->parent_sk == NULL) {
		if(sk->zapped == 0) {
			lpxitf_remove_sock(sk);
		}
	} else
	{ 
		struct sock *parent_sk;
		struct sock *s;

		parent_sk = LPX_STREAM_OPT(sk)->parent_sk;
		LPX_STREAM_OPT(sk)->parent_sk = NULL;

		s = LPX_STREAM_OPT(parent_sk)->child_sklist;
		if(s == sk) {
			LPX_STREAM_OPT(parent_sk)->child_sklist = s->next;
			s = NULL;
		}

		while(s && s->next) {
			if(s->next == sk) {
				s->next = sk->next;
				break;
			}
			s = sk->next;
		}


	}
	
	while((skb = skb_dequeue(&sk->back_log)) != NULL) {
		back_log ++;
		kfree_skb(skb);
	}
	
	while((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
		receive_queue ++;
		kfree_skb(skb);
	}

	while((skb = skb_dequeue(&sk->write_queue)) != NULL) {
		write_queue ++;
		kfree_skb(skb);
	}

	while((skb = skb_dequeue(&LPX_STREAM_OPT(sk)->retransmit_queue)) != NULL) {
		retransmit_queue ++;
		kfree_skb(skb);
	}

	RELEASE_SOCKLOCK(sk,flags);

	NbDebugPrint(2, ("back_log = %d, receive_queue = %d, write_queue = %d, retransmit_queue = %d\n", 
			back_log, receive_queue, write_queue, retransmit_queue));

	sk_free(sk);

}


int lpx_stream_bind(struct sock *sock, struct sockaddr *uaddr, int addr_len)
{
//	struct sock *sk = sock->sk;
	struct sock *sk = sock;
	int err;

//	MpDebugPrint((1, "lpx_stream_bind: lock_sock = %d\n", atomic_read(&sk->sock_readers)));
	lock_sock(sk);

	err = lpx_dgram_bind(sock, uaddr, addr_len);

//	MpDebugPrint((1, "lpx_stream_bind: release_sock = %d\n", atomic_read(&sk->sock_readers)));
	release_sock(sk);

	return err;
}


/* Build a connection to an SMP socket */
int lpx_stream_connect(struct sock *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
//	struct sock 		*sk = sock->sk;
	struct sock 		*sk = sock;	
	struct sockaddr_lpx 	saddr;
	int 			size, err = 0;
	KIRQL			oldflags = 0;	
	LARGE_INTEGER	TimeOut;
	NTSTATUS		status;

	NbDebugPrint(2, ("lpx_stream_connect\n"));

	if(sock->type != SOCK_SEQPACKET && sock->type != SOCK_STREAM)
		return -EOPNOTSUPP;

	size = sizeof(saddr);
	err  = lpx_dgram_getname(sock, (struct sockaddr *)&saddr, &size, 0);
	if(err) 
 		goto out;
		
	memcpy(LPX_OPT(sk)->source_addr.node, saddr.slpx_node, LPX_NODE_LEN);
	LPX_OPT(sk)->source_addr.port = saddr.slpx_port;
	
	err = lpx_dgram_connect(sock, uaddr, addr_len, flags);
	if(err) 
 		goto out;

	sk->state	 = SMP_SYN_SENT;

	/* Send Connection request */
	err = lpx_stream_transmit(sk, NULL, CONREQ, 0);			
	
	if(err)
 		goto out;

	TimeOut.QuadPart = - MAX_CONNECT_TIME;

	TimeOut.QuadPart = - 5L * HZ;

	do {

#if 0
		if(order_count < 100) order_list[order_count++] = 'S';		
#endif

		status = KeWaitForSingleObject(
				&sk->ArrivalEvent,
				Executive,
				KernelMode,
				FALSE,
				&TimeOut
			);		

		switch(status)	{
			case STATUS_SUCCESS:
				KeClearEvent(&sk->ArrivalEvent);
			
				if(sk->err) {
					err = sock_error(sk);
					goto out;
				}

				if(sk->dead) {
					err = -ERESTARTSYS;
					goto out;
				}					

				break;
			case STATUS_TIMEOUT:
				
#if 0
				if(order_count < 100) order_list[order_count++] = 'O';						
#endif
				if(sk->state == SMP_SYN_SENT) {
					sk->err = 1;
					err = -ETIME;
				}
				
				while(1) ;
				continue;
			default:
				break;
		}				
	} while(sk->state == SMP_SYN_SENT) ;

	NbDebugPrint(0, ("Exit connect_loop\n"));

	if(sk->err) {
		err = sock_error(sk);
	}
out:	

	NbDebugPrint(2, ("sk->err = %lx, err = %lx\n", sk->err, err));

	return err;
}


#define BACKLOG(sk)	sk->ack_backlog
#define BACKLOGMAX(sk)	sk->max_ack_backlog

/* Accept a pending SMP connection */
int lpx_stream_accept(struct sock *sock, struct sock *newsock, int flags)
{
	struct sock 		*sk, *newsk;
	int 			err;

	UNREFERENCED_PARAMETER(flags);

	NbDebugPrint(2, ("lpx_stream_accept\n"));

//	if(sock->sk == NULL)
//		return -EINVAL;

//	if(flags & O_NONBLOCK)
//		return -EOPNOTSUPP;

	if(sock->type != SOCK_SEQPACKET && sock->type != SOCK_STREAM)
		return -EOPNOTSUPP;

//	sk = sock->sk;
	sk = sock;

//	MpDebugPrint((1, "lpx_stream_accept: lock_sock = %d\n", atomic_read(&sk->sock_readers)));
	lock_sock(sk);

	if(sk->state != SMP_LISTEN) {
		release_sock(sk);
		return -EINVAL;
	}
	
//	newsk = newsock->sk;
	newsk = newsock;
	
	LPX_OPT(newsk)->interface = LPX_OPT(sk)->interface;
	LPX_OPT(newsk)->source_addr = LPX_OPT(sk)->source_addr;
	
	LPX_STREAM_OPT(newsk)->parent_sk = sk;
	newsk->next = NULL;

	if(LPX_STREAM_OPT(sk)->child_sklist == NULL)
		LPX_STREAM_OPT(sk)->child_sklist = newsk;
	else {
		struct sock *s;

		for(s = LPX_STREAM_OPT(sk)->child_sklist; s->next != NULL; s = s->next);
		s->next = newsk;
	}

	newsk->zapped = 0;
	newsk->state = SMP_LISTEN;
	
//	MpDebugPrint((1, "lpx_stream_accept: lock_sock = %d\n", atomic_read(&sk->sock_readers)));
	release_sock(sk);
	
//	MpDebugPrint((1, "lpx_stream_accept: lock_sock(new) = %d\n", atomic_read(&newsk->sock_readers)));
	lock_sock(newsk);

	while(newsk->state == SMP_LISTEN) {	

		if(sk->err) {
			err = sock_error(sk);
//			MpDebugPrint((1, "lpx_stream_accept: release_sock(new) = %d\n", atomic_read(&newsk->sock_readers)));
			release_sock(newsk);
			return err;
		}

		if(newsk->dead) {
			err = -ERESTARTSYS;
//			MpDebugPrint((1, "lpx_stream_accept: release_sock(new) = %d\n", atomic_read(&newsk->sock_readers)));
			release_sock(newsk);
			return err;
		}

//		MpDebugPrint((1, "lpx_stream_accept: release_sock(new) = %d\n", atomic_read(&newsk->sock_readers)));
		release_sock(newsk);
//		ACQUIRE_SOCKLOCK(sk, oldflags);
		if(newsk->state == SMP_LISTEN) {
//			if(newsk->sleep)
//				interruptible_sleep_on(newsk->sleep);
		}
//		RELEASE_SOCKLOCK(sk, oldflags);
//		MpDebugPrint((1, "lpx_stream_accept: lock_sock(new) = %d\n", atomic_read(&newsk->sock_readers)));
		lock_sock(newsk);
	} 

//	MpDebugPrint((1, "lpx_stream_accept: release_sock(new) = %d\n", atomic_read(&newsk->sock_readers)));
	release_sock(newsk);

	return 0; 
}


int lpx_stream_getname (struct sock *sock, struct sockaddr *uaddr, int *usockaddr_len, int peer)
{
	int err;

	err = lpx_dgram_getname(sock, uaddr, usockaddr_len, peer);

	return err;
}

int lpx_stream_ioctl(struct sock *sock, unsigned int cmd, unsigned long arg)
{
//	struct sock *sk = sock->sk;
	struct sock *sk = sock;

	UNREFERENCED_PARAMETER(arg);

	switch(cmd) {
		case IOCTL_DEL_TIMER:
			KeCancelTimer(&LPX_STREAM_OPT(sk)->IopTimer);
			break;
		default:
			break;
	}

	return 0;
}

int lpx_stream_sendmsg(struct sock *sock, struct msghdr *msg, int len)
{
//	struct sock *sk = sock->sk;
	struct sock *sk = sock;
	int retval;
	
	UNREFERENCED_PARAMETER(len);

	if(sk == NULL)
		return (-EINVAL);

	lock_sock(sk);

	if(sk->state != SMP_ESTABLISHED) {
		retval = -EPIPE;
		goto out;
	}

	if (sk->shutdown & SEND_SHUTDOWN) {
		retval = -EPIPE;
		goto out;
	}
	if(sk->err) {
		retval = sock_error(sk);
		goto out;
	}

	retval = lpx_stream_do_sendmsg(sk, msg);
out:
	release_sock(sk);

	NbDebugPrint(4, ("returned lpx_stream_sendmsg\n"));

	return retval;
}


int lpx_stream_do_sendmsg(struct sock *sk, struct msghdr *msg)
{
	struct iovec	*iov;
	struct sk_buff 	*skb;
	int 		iovlen, flags;
	int 		dev_mtu;
	int 		mss_now;
	int 		err, copied;

	NbDebugPrint(3, ("called lpx_stream_do_sendmsg\n"));

	err = 0;
	flags = msg->msg_flags;	

	if (flags & ~MSG_DONTWAIT) {
		NbDebugPrint(3, ("lpx_stream_do_sendmsg socket is not MSG_DONTWAIT flags = 0x%x\n", flags));
		err = -EINVAL;
		goto out;
	}

	if(sk->state != SMP_ESTABLISHED) {
		NbDebugPrint(1, ("lpx_stream_do_sendmsg socket is not connected\n"));
		err = -ENOTCONN;
		goto out;
	}

//	if(!sk->sock) {
//		NbDebugPrint(1, ("lpx_stream_do_sendmsg socket is NULL\n"));
//		return -EPIPE;
//	}

	dev_mtu = lpxitf_mtu(sk);
	mss_now = dev_mtu - sizeof(struct lpxhdr);

	iovlen = msg->msg_iovlen;
	iov = msg->msg_iov;
	copied = 0;

	while(--iovlen >= 0) {
		int seglen = iov->iov_len;
		unsigned char *from = iov->iov_base;

		iov++;

		NbDebugPrint(4, ("called lpx_stream_do_sendmsg in while\n"));

		while(seglen > 0) {
			int copy, tmp;
			int offset;

			if (err)
				goto do_fault2;

			/* Stop on errors. */
			if (sk->err)
				goto do_sock_err;

			/* Make sure that we are established. */
			if (sk->shutdown & SEND_SHUTDOWN)
				goto do_shutdown;

			copy = mss_now;
			if(copy > seglen)
				copy = seglen;

  			offset = lpxitf_offset(sk);
			if(offset < 0) {
				err = offset;
				goto out;
			}
			tmp = offset + sizeof(struct lpxhdr) + copy;

			if(!(flags & MSG_DONTWAIT))
				NbDebugPrint(3, ("flag is not MSG_DONTWAIT\n"));

			skb = sock_wmalloc(sk, tmp, 0, 0);
			if(!skb) {
				NbDebugPrint(2, ("after sock_wmalloc skb = %p\n", skb));
				if(flags & MSG_DONTWAIT) {
					err = -EAGAIN;
					goto do_interrupted;
				}
				wait_for_lpx_stream_memory(sk);
				continue;
			}

			seglen -= copy;

			skb->sk = sk;
			skb_reserve(skb, offset);
			skb->h.raw = skb->nh.raw = skb_put(skb, sizeof(struct lpxhdr));

			if(copy_from_user(skb_put(skb, copy), from, copy) == NULL)
				goto do_fault;

			err = lpx_stream_transmit(sk, skb, DATA, copy);
			if(err) {
				NbDebugPrint(1, ("lpx_stream_tramsit error\n"));
 				err = -EAGAIN;
				goto out;
			}
			from += copy;
			copied += copy;
			
		}
	}
	sk->err = 0;
	err = copied;
	goto out;

do_sock_err:
	if(copied)
		err = copied;
	else
		err = sock_error(sk);
	goto out;

do_shutdown:
	if(copied)
		err = copied;
	else {

		err = -EPIPE;
	}
	goto out;
do_interrupted:
	if(copied)
		err = copied;
	goto out;
do_fault:
	kfree_skb(skb);
do_fault2:
	err = -EFAULT;
out:
	return err;
}


void wait_for_lpx_stream_memory(struct sock * sk)
{
//		MpDebugPrint((1, "wait_for_lpx_stream_memory: release_sock = %d\n", atomic_read(&sk->sock_readers)));
        release_sock(sk);
        if (!lpx_stream_memory_free(sk)) {
//                sk->sndbuf *= 2;
        }
//		MpDebugPrint((1, "wait_for_lpx_stream_memory: lock_sock = %d\n", atomic_read(&sk->sock_readers)));
        lock_sock(sk);
        return;

        if (!lpx_stream_memory_free(sk)) {
                for (;;) {
                        if (lpx_stream_memory_free(sk))
                                break;
                        if (sk->shutdown & SEND_SHUTDOWN)
                                break;
                        if (sk->err)
                                break;
//                        schedule();
                }
        }
//		MpDebugPrint((1, "wait_for_lpx_stream_memory: lock_sock = %d\n", atomic_read(&sk->sock_readers)));
        lock_sock(sk);
}


__inline int lpx_stream_memory_free(struct sock *sk)
{
		UNREFERENCED_PARAMETER(sk);
//        return atomic_read(&sk->wmem_alloc) < sk->sndbuf;
		return 1;
}


int lpx_stream_transmit(struct sock *sk, struct sk_buff *skb, int type, int len)
{
	struct lpxhdr		*lpxhdr;

	NbDebugPrint(3, ("called lpx_stream_transmit\n"));

	if(!skb) {
		int offset  = lpxitf_offset(sk);
		int size    = offset + sizeof(struct lpxhdr);

		if(offset < 0)
			return offset;

		skb = sock_wmalloc(sk, size, 1, 0);
		if(!skb) {
			NbDebugPrint(0, ("skb not allocated\n"));
			return -ENOMEM;
		}

		skb_reserve(skb, offset);
		skb->h.raw = skb->nh.raw = skb_put(skb, sizeof(struct lpxhdr));
	}

	lpxhdr = (struct lpxhdr *)skb->nh.raw;

	if(type == DATA)
		lpxhdr->pu.pktsize		= HTONS(LPX_PKT_LEN+len);
	else
		lpxhdr->pu.pktsize		= HTONS(LPX_PKT_LEN);

	lpxhdr->pu.p.type =		LPX_TYPE_STREAM;

//	lpxhdr->checksum		= 0;
	lpxhdr->dest_port		= LPX_OPT(sk)->dest_addr.port;
	lpxhdr->source_port	= LPX_OPT(sk)->source_addr.port;
//	lpxhdr->type		= LPX_TYPE_SEQPACKET;

	lpxhdr->u.s.sequence		= HTONS(LPX_STREAM_OPT(sk)->sequence);
	lpxhdr->u.s.ackseq		= HTONS(LPX_STREAM_OPT(sk)->rmt_seq);
	lpxhdr->u.s.server_tag		= LPX_STREAM_OPT(sk)->server_tag;
//	lpxhdr->u.s.sconn		= LPX_STREAM_OPT(sk)->source_connid;
//	lpxhdr->u.s.dconn		= LPX_STREAM_OPT(sk)->dest_connid;
//	lpxhdr->u.s.allocseq		= HTONS(LPX_STREAM_OPT(sk)->alloc);

	switch(type) {

	case CONREQ:	/* Connection Request */
	case DATA:	/* Data */
	case DISCON:	/* Inform Disconnection */

		if(type == CONREQ) {
			lpxhdr->u.s.lsctl 	= HTONS(LSCTL_CONNREQ | LSCTL_ACK);
		} else if(type == DATA) {
			lpxhdr->u.s.lsctl 	= HTONS(LSCTL_DATA | LSCTL_ACK);
		} else if(DISCON) {
			lpxhdr->u.s.sequence	= HTONS(LPX_STREAM_OPT(sk)->fin_seq);
			lpxhdr->u.s.lsctl 	= HTONS(LSCTL_DISCONNREQ | LSCTL_ACK);
			LPX_STREAM_OPT(sk)->fin_seq++;
		}

		LPX_STREAM_OPT(sk)->sequence++;

//		NbDebugPrint(3, ("type = %d, LPX_STREAM_OPT(sk)->rmt_ack = %d, lpxhdr->sequence = %d, LPX_STREAM_OPT(sk)->sequence = %d\n",
//				type, LPX_STREAM_OPT(sk)->rmt_ack, NTOHS(lpxhdr->u.s.sequence), LPX_STREAM_OPT(sk)->sequence));

		NbDebugPrint(3,("T:lsctl = %04X, sk->sequence = %d, sk->rmt_seq = %d, sk->rmt_ack = %d, lpxhdr->sequence = %d, lpxhdr->ackseq = %d, jiffies = %I64X\n",
			NTOHS(lpxhdr->u.s.lsctl), LPX_STREAM_OPT(sk)->sequence, LPX_STREAM_OPT(sk)->rmt_seq, LPX_STREAM_OPT(sk)->rmt_ack, NTOHS(lpxhdr->u.s.sequence), NTOHS(lpxhdr->u.s.ackseq), jiffies));

		if(!skb_queue_empty(&sk->write_queue) || !lpx_stream_snd_test(sk, skb)) {
			NbDebugPrint(0,("queued to write_queue!!!!!\n\n"));
			skb_queue_tail(&sk->write_queue, skb);
			return 0;
		}

		break;

	case ACKREQ:	/* ACKREQ */

		lpxhdr->u.s.lsctl 	= HTONS(LSCTL_ACKREQ | LSCTL_ACK);

		break;

	case ACK:	/* ACK */

		lpxhdr->u.s.lsctl 	= HTONS(LSCTL_ACK);

		break;

	default:
		return -EOPNOTSUPP;
	}

	return lpx_stream_route_skb(sk, skb, type);
}


int lpx_stream_route_skb(struct sock *sk, struct sk_buff *skb, int type)
{
	struct sk_buff	*skb2;
	int 		err = 0;
	lpx_interface	*interface;

	NbDebugPrint(4, ("called lpx_stream_route_skb: skb = %08X\n", skb));

	if (sk->zapped == 1 && (!LPX_OPT(sk)->virtual_mapping)) {
		NbDebugPrint(0, ("lpx_send_skb error\n"));
		kfree_skb(skb);
		return -1;
	}

	switch(type) {

	case CONREQ:		
	case DATA:		
	case DISCON:

		skb2 = skb_clone(skb, 0);

		if(!skb2) {
			skb_queue_tail(&LPX_STREAM_OPT(sk)->retransmit_queue, skb);
			if(skb == skb_peek(&LPX_STREAM_OPT(sk)->retransmit_queue)) {
				LPX_STREAM_OPT(sk)->retransmit_timeout = jiffies + lpx_stream_calc_rtt(sk);
			}

			return 0;
		}
				
		skb2->sk = skb->sk;
//	NbDebugPrint(3, ("called lpx_stream_route_skb\n"));
		skb_queue_tail(&LPX_STREAM_OPT(sk)->retransmit_queue, skb);
//	NbDebugPrint(3, ("called lpx_stream_route_skb\n"));
//		if(skb == skb_peek(&LPX_STREAM_OPT(sk)->retransmit_queue)) {
			LPX_STREAM_OPT(sk)->retransmit_timeout = jiffies + lpx_stream_calc_rtt(sk);
//		}

		skb = skb2;

	case RETRAN:
		
		LPX_STREAM_OPT(sk)->latest_sendtime = jiffies;

	case ACK:
	case ACKREQ:
	
	default:

		interface = lpxitf_find_using_node(LPX_OPT(sk)->source_addr.node);		
		if(LPX_OPT(sk)->virtual_mapping) 
		{
			lpx_interface *p_if;
			struct sk_buff *skb2;

			for(p_if = lpx_interfaces; p_if; p_if = p_if->itf_next) {				
				skb2 = skb_clone(skb, 0);	
				skb2->sk = skb->sk;
				if(skb2) {
					lpxitf_send(p_if, skb2, LPX_OPT(sk)->dest_addr.node, type);
				}
			}
			NbDebugPrint(3, ("lpxitf_send: skb = %08X\n", skb));
			kfree_skb(skb);
			return 0;
		}

		if(interface == NULL) {
			kfree_skb(skb);
			return -1;
		}
		lpxitf_send(interface, skb, LPX_OPT(sk)->dest_addr.node, type);
	}

	return err;
}


__inline unsigned long lpx_stream_calc_rtt(struct sock *sk)
{
/*
	if(LPX_STREAM_OPT(sk)->retransmits == 0)
		return LPX_STREAM_OPT(sk)->interval_time * 10;
	if(LPX_STREAM_OPT(sk)->retransmits < 10)
		return LPX_STREAM_OPT(sk)->interval_time * LPX_STREAM_OPT(sk)->retransmits * 1000;

	return (MAX_RETRANSMIT_DELAY);
*/
	UNREFERENCED_PARAMETER(sk);

	return RETRANSMIT_TIME;
}


int lpx_stream_snd_test(struct sock *sk, struct sk_buff *skb)
{
	USHORT in_flight;
	struct lpxhdr *lpxhdr;

	lpxhdr = (struct lpxhdr *)skb->nh.raw;
	in_flight = NTOHS(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->rmt_ack;
	if((in_flight >= LPX_STREAM_OPT(sk)->max_flights)) {
		NbDebugPrint(1, ("in_flight = %d\n", in_flight));
		return 0;
	}

	return 1;
}

/* Send message/lpx data to user-land */
int lpx_stream_recvmsg(struct sock *sock, struct msghdr *msg, int size, int flags, PLARGE_INTEGER	TimeOut)
{
	struct sk_buff *skb;
	struct lpxhdr *lpxhdr;
	struct sock *sk;
	struct sockaddr_lpx *saddr = (struct sockaddr_lpx *)msg->msg_name;
	int copied, err = 0;
	int total = 0;
	NTSTATUS status;
	LARGE_INTEGER timeout;
	KIRQL	Irql = PASSIVE_LEVEL;
	
	NbDebugPrint(3, ("called lpx_stream_recvmsg\n"));

	sk = sock;
	if(sk == NULL)
		return (-EINVAL);

	if(sk->zapped) {
		NbDebugPrint(3, ("sk->zapped called lpx_stream_recvmsg\n"));
		return (-ENOTCONN); /* Socket not bound */
	}
	
	if(!TimeOut) {		
		timeout.QuadPart = - 2 * HZ;
		TimeOut = &timeout;
	}
	else {		
		TimeOut->QuadPart = - 2 * HZ;		
	}

	while(size) {
		/* Socket errors? */
		err = sock_error(sk);
		if(err) {
			NbDebugPrint(0, ("sock error signal_pending\n"));					
			goto out;
		}

		/* Socket shut down? */
		if(sk->shutdown & RCV_SHUTDOWN) {
			NbDebugPrint(0, ("RCV_SHUTDOOWN sock error signal_pending\n"));
			err = -ESHUTDOWN;
			goto out;
		}

		/* User doesn't want to wait */
		if(flags&MSG_DONTWAIT) {
			NbDebugPrint(0, ("MSG_DONTWAIT\n"));
			err = -EAGAIN;
			goto out;
		}	

		while(!skb_queue_empty(&sk->receive_queue)) {			

			skb = skb_peek(&sk->receive_queue);
			if(skb == NULL)	continue;

			lpxhdr	= (struct lpxhdr *)skb->nh.raw;
			if(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_DISCONNREQ) {
				skb_unlink(skb);
				kfree_skb(skb);
				goto out;
			}

			copied 	= skb->len;
			if(copied > size)
			{
				NbDebugPrint(1, ("It's long skbuff, copied = %d, size = %d\n", copied, size));
				copied = size;
			}

			err = memcpy_toiovec(msg->msg_iov, skb->data, copied);
			if(err) {
				NbDebugPrint(1, ("memcpy_to error\n"));
				kfree_skb(skb);
				skb_unlink(skb);
 				goto out;
			}

			msg->msg_namelen = sizeof(*saddr);
			if(saddr) {
				saddr->slpx_family	= AF_LPX;
				saddr->slpx_port	= lpxhdr->source_port;
				memcpy(saddr->slpx_node, skb->mac.ethernet->h_dest, LPX_NODE_LEN);
			}

			NbDebugPrint(1, ("skb->len = %d, copied = %d, skb->data = %p, skb->nh.raw = %p\n", 
				skb->len, copied, skb->data, skb->nh.raw));

			if(skb->len == copied) {
				skb_unlink(skb);
				kfree_skb(skb);
			} else
				skb_pull(skb, copied);

			NbDebugPrint(1, ("size = %d, copied = %d\n", size, copied));
			total += copied;
			size -= copied;

			if(size == 0) 
				goto out;
		}
	
		status = KeWaitForSingleObject(
							&sk->ArrivalEvent,
							Executive,
							KernelMode,
							FALSE,
							TimeOut
						);

		

		switch(status)	{
			case STATUS_SUCCESS:		
				KeClearEvent(&sk->ArrivalEvent);
				break;			

			case STATUS_TIMEOUT:		
				sk->err = 1;
				err = -ETIME;
				goto out;
		}
	}
					
out:

	if(total) err = total;

//	MpDebugPrint((1, "lpx_stream_recvmsg: release_sock = %d\n", atomic_read(&sk->sock_readers)));	
	return err;
}


void lpx_stream_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct lpxhdr	*lpxhdr;
	struct sock 	*child_sk;
	struct sock 	*receive_sk;


	NbDebugPrint(2, ("called lpx_stream_rcv, sk = %p, skb->nh.raw = %p, skb->data = %p, skb->len = %d\n", 
			sk, skb->nh.raw, skb->data, skb->len));

#if 0
	if(order_count < 100) order_list[order_count++] = 'X';	
#endif

//	NbDebugPrint(0, ("called lpx_stream_rcv, sk = %p, skb->nh.raw = %p, skb->data = %p, skb->len = %d\n", 
//			sk, skb->nh.raw, skb->data, skb->len));
	

	lpxhdr = (struct lpxhdr *)skb->nh.raw;
/*
	{	
		USHORT *temp = (USHORT *) lpxhdr;
		int i;

		for(i=0;i<40;i++) {
			NbDebugPrint(2, ("%04x ", HTONS(temp[i])));
			if((i+1)%8 == 0) NbDebugPrint(2, ("\n"));
		}
	}	
*/

	if((NTOHS(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK)) < LPX_PKT_LEN) {
		kfree_skb(skb);
		return;
	}

	if(skb->len < (NTOHS(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK))) {
		kfree_skb(skb);
		return;
	}

	if(skb->len != (NTOHS(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK)))
		skb_trim(skb, (NTOHS(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK)));

	skb_pull(skb, LPX_PKT_LEN);


	if(LPX_STREAM_OPT(sk)->child_sklist == NULL) {
		receive_sk = sk;
		goto no_child;
	}

	child_sk = LPX_STREAM_OPT(sk)->child_sklist;
	receive_sk = NULL;

	while(child_sk != NULL) {
		if((memcmp(LPX_OPT(child_sk)->dest_addr.node, skb->mac.ethernet->h_source, 6) == 0)
		&& (LPX_OPT(child_sk)->dest_addr.port == lpxhdr->source_port))
		{
			NbDebugPrint(4, ("find called lpx_stream_rcv, child_sk = %p\n", child_sk));
			receive_sk = child_sk;
			break;
		}
		child_sk = child_sk->next;
	}
	
	NbDebugPrint(4, ("child called lpx_stream_rcv, child_sk = %p\n", sk));
	
	if(receive_sk == NULL) {
		child_sk = LPX_STREAM_OPT(sk)->child_sklist;
		while(child_sk != NULL) {

			NbDebugPrint(1, ("called lpx_stream_rcv, child_sk = %p\n", child_sk));
			if(child_sk->state == SMP_LISTEN)
			{
				receive_sk = child_sk;
				if(!LPX_OPT(receive_sk)->interface) {
					LPX_OPT(receive_sk)->interface = LPX_OPT(sk)->interface;
					memcpy(LPX_OPT(receive_sk)->source_addr.node, 
							LPX_OPT(sk)->source_addr.node, LPX_NODE_LEN);
				}
				break;
			}
			child_sk = child_sk->next;
		}
	}
	
	
	if(receive_sk == NULL) {
		kfree_skb(skb);
		return;
	}
		
		
no_child:
	
	if(receive_sk->state == SMP_CLOSE) {
		NbDebugPrint(1, ("SMP_CLOSED lpxhdr->sequence = %x\n", lpxhdr->u.s.sequence));
		kfree_skb(skb);
		return;
	}

	NbDebugPrint(1, ("sock_readers = %d\n", receive_sk->sock_readers)) ;

#if 0
	if(order_count < 100) order_list[order_count++] = 'R';		
#endif

	if(!receive_sk->sock_readers) {
		lpx_stream_do_rcv(receive_sk, skb);
	} else {
		__skb_queue_tail(&receive_sk->back_log, skb);
		NbDebugPrint(1, ("lpx_stream_rcv: sock_readers = %d\n", receive_sk->sock_readers)) ;

	}
	return; 
}

 
/* SMP lpx receive engine */
int lpx_stream_do_rcv(struct sock* sk, struct sk_buff *skb)
{
	struct lpxhdr 	*lpxhdr = (struct lpxhdr *)skb->nh.raw;
	char			skbuff_consumed = 0;
	int			err;

//	NbDebugPrint(0, ("called lpx_stream_do_rcv, port = %x, sk->state = %x\n", LPX_OPT(sk)->source_addr.port, sk->state));

//	NbDebugPrint(0, ("R:lsctl = %04X, sk->sequence = %d, sk->rmt_seq = %d sk->rmt_ack = %d, lpxhdr->sequence = %d, lpxhdr->ackseq = %d\n",
//			NTOHS(lpxhdr->u.s.lsctl), LPX_STREAM_OPT(sk)->sequence, LPX_STREAM_OPT(sk)->rmt_seq, LPX_STREAM_OPT(sk)->rmt_ack, NTOHS(lpxhdr->u.s.sequence), NTOHS(lpxhdr->u.s.ackseq)));
	
	LPX_STREAM_OPT(sk)->alive_retries = 0;

	if(((SHORT)(NTOHS(lpxhdr->u.s.ackseq) - LPX_STREAM_OPT(sk)->sequence)) > 0) {
		kfree_skb(skb);
		return 0;
	}

	if(NTOHS(lpxhdr->u.s.sequence) != LPX_STREAM_OPT(sk)->rmt_seq 
		&& !(NTOHS(lpxhdr->u.s.lsctl) ==  LSCTL_ACK 
			&& ((SHORT)(NTOHS(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->rmt_seq) > 0)) 
		&& !(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_DATA)) 
	{
		kfree_skb(skb);
		return 0;
	}	

	switch(sk->state) {

	case SMP_CLOSE:

		break;

	case SMP_LISTEN:

		if(LPX_STREAM_OPT(sk)->parent_sk == NULL) {
			break;
		}

		switch(NTOHS(lpxhdr->u.s.lsctl)) 
		{
		case LSCTL_CONNREQ:
		case LSCTL_CONNREQ | LSCTL_ACK:

			NbDebugPrint(1, ("SMP_LISTEN\n"));
			
//			if(sk->sock)
//				sk->sock->state = SS_CONNECTING;
			sk->state 	= SMP_SYN_RECV;

			((struct lpx_stream_opt *)(&sk->tp_pinfo.af_lpx_stream))->rmt_seq ++;
			memcpy(LPX_OPT(sk)->dest_addr.node, skb->mac.ethernet->h_source, 6);
			LPX_OPT(sk)->dest_addr.port = lpxhdr->source_port;
			LPX_STREAM_OPT(sk)->server_tag = lpxhdr->u.s.server_tag;
			lpx_stream_transmit(sk, NULL, CONREQ, 0);   /* Connection REQUEST */

			break;
		}

		break;

	case SMP_SYN_SENT:

		switch(NTOHS(lpxhdr->u.s.lsctl)) {

		case LSCTL_CONNREQ:
		case LSCTL_CONNREQ | LSCTL_ACK:
		
			NbDebugPrint(0, ("SMP_SYN_SENT\n"));
			
			LPX_STREAM_OPT(sk)->rmt_seq ++;
			LPX_STREAM_OPT(sk)->server_tag = lpxhdr->u.s.server_tag;

			sk->state = SMP_ESTABLISHED;

			if(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_ACK) { 
				LPX_STREAM_OPT(sk)->rmt_ack = NTOHS(lpxhdr->u.s.ackseq); // must be 1
				lpx_stream_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);
			}

			lpx_stream_transmit(sk, NULL, ACK, 0);
			if(!sk->dead) {								
				sk->state_change(sk);
			}

			break;

		default:

			break;
		}

		break;

	case SMP_SYN_RECV:

		NbDebugPrint(1, ("SMP_SYN_RECV\n"));

		if(!(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_ACK)) {
			NbDebugPrint(2, ("SMP_SYN_RECV not ack\n"));
			break;
		}
 
		if(NTOHS(lpxhdr->u.s.ackseq) < 1) {
			NbDebugPrint(2, ("SMP_SYN_RECV ackseq = %x\n", 
				NTOHS(lpxhdr->u.s.ackseq)));
			break;
		}

	//	if(sk->sock)
	//		sk->sock->state = SS_CONNECTED;
		sk->state = SMP_ESTABLISHED;
		NbDebugPrint(2, ("SMP_SYN_RECV wake\n"));
		if(!sk->dead)
			sk->state_change(sk);

		goto established;

	case SMP_ESTABLISHED:

established:
	//	NbDebugPrint(1, ("SMP_ESTABLISHED\n"));
	
		if(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_ACK) {
			LPX_STREAM_OPT(sk)->rmt_ack = NTOHS(lpxhdr->u.s.ackseq);
			NbDebugPrint(3, ("ackseq = %x\n", NTOHS(lpxhdr->u.s.ackseq)));
			lpx_stream_retransmit_chk(sk, NTOHS(lpxhdr->u.s.ackseq), ACK);
		}
		NbDebugPrint(2, ("SMP_ESTABLISHED2\n"));

	switch(NTOHS(lpxhdr->u.s.lsctl)) {
	
		case LSCTL_ACKREQ:
		case LSCTL_ACKREQ | LSCTL_ACK:

//			NbDebugPrint(1, ("SMP_ACKREQ\n"));
			lpx_stream_transmit(sk, NULL, ACK, 0);
			break;

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:
//			NbDebugPrint(1, ("SMP_DATA\n"));
		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:
//			NbDebugPrint(1, ("SMP_DISCONNREQ\n"));
			if((NTOHS(lpxhdr->u.s.lsctl) == (LSCTL_DISCONNREQ))
				&& (NTOHS(lpxhdr->u.s.lsctl)  == (LSCTL_DISCONNREQ | LSCTL_ACK))) {
				NbDebugPrint(1, ("!!!!!!!!!!!!!!!!!!!!!! DISCONNECT RECEIVCED !!!!!!!!!!!!!!!!!!"));
			}
			if(NTOHS(lpxhdr->u.s.sequence) != LPX_STREAM_OPT(sk)->rmt_seq) {
				lpx_stream_transmit(sk, NULL, ACK, 0);
				break;
			}			
			
			err = sock_queue_rcv_skb(sk, skb);
			if(err) {
				NbDebugPrint(0, ("RECV:receive data error\n"));
				break;
			}

			LPX_STREAM_OPT(sk)->rmt_seq ++;

			skbuff_consumed = 1;
			lpx_stream_transmit(sk, NULL, ACK, 0);

			if(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_DISCONNREQ) {
				sk->state = SMP_CLOSE_WAIT;
				sk->shutdown = SHUTDOWN_MASK;
			}

			break;

		default:
			NbDebugPrint(1, ("SMP_DEFAULT\n"));

			break;
		}

		break;

	case SMP_LAST_ACK:
		
		switch(NTOHS(lpxhdr->u.s.lsctl)) {

		case LSCTL_ACK:

			LPX_STREAM_OPT(sk)->rmt_ack = NTOHS(lpxhdr->u.s.ackseq);
			lpx_stream_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

			if(LPX_STREAM_OPT(sk)->rmt_ack == LPX_STREAM_OPT(sk)->fin_seq) {
				sk->state = SMP_CLOSE;
			}
			break;
	
		default:
			break;

		}

		break;

	case SMP_FIN_WAIT1:

		switch(NTOHS(lpxhdr->u.s.lsctl)) {

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:

			LPX_STREAM_OPT(sk)->rmt_seq ++;

			if(!(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_ACK))
				break;

		case LSCTL_ACK:

			LPX_STREAM_OPT(sk)->rmt_ack = NTOHS(lpxhdr->u.s.ackseq);
			lpx_stream_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

			if(LPX_STREAM_OPT(sk)->rmt_ack == LPX_STREAM_OPT(sk)->fin_seq)
				sk->state = SMP_FIN_WAIT2;

			break;
	
		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			LPX_STREAM_OPT(sk)->rmt_seq ++;

			lpx_stream_transmit(sk, NULL, ACK, 0);
			sk->state = SMP_CLOSING;

			if(NTOHS(lpxhdr->u.s.lsctl) & LSCTL_ACK) {
				LPX_STREAM_OPT(sk)->rmt_ack = NTOHS(lpxhdr->u.s.ackseq);
				lpx_stream_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

				if(LPX_STREAM_OPT(sk)->rmt_ack == LPX_STREAM_OPT(sk)->fin_seq)
					sk->state = SMP_TIME_WAIT;
			}

			break;
		}

		break;

	case SMP_FIN_WAIT2:
		
		switch(NTOHS(lpxhdr->u.s.lsctl)) {

		case LSCTL_DATA:
		case LSCTL_DATA | LSCTL_ACK:

			LPX_STREAM_OPT(sk)->rmt_seq ++;

			break;

		case LSCTL_DISCONNREQ:
		case LSCTL_DISCONNREQ | LSCTL_ACK:

			LPX_STREAM_OPT(sk)->rmt_seq ++;

			if(LPX_STREAM_OPT(sk)->rmt_ack != LPX_STREAM_OPT(sk)->fin_seq) // impossible
				break;

			NbDebugPrint(3, ("SMP_FIN_WAIT2\n"));
			sk->state = SMP_TIME_WAIT;
			LPX_STREAM_OPT(sk)->time_wait_timeout = jiffies + TIME_WAIT_INTERVAL;

			lpx_stream_transmit(sk, NULL, ACK, 0);

			NbDebugPrint(3, ("SMP_FIN_WAIT2 end\n"));
			break;
		
		default:
			break;
		}

		break;

	case SMP_CLOSING:
		
		switch(NTOHS(lpxhdr->u.s.lsctl)) {

		case LSCTL_ACK:
		
			LPX_STREAM_OPT(sk)->rmt_ack = NTOHS(lpxhdr->u.s.ackseq);
			lpx_stream_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

			if(LPX_STREAM_OPT(sk)->rmt_ack != LPX_STREAM_OPT(sk)->fin_seq)
				break;

			sk->state = SMP_TIME_WAIT;
			LPX_STREAM_OPT(sk)->time_wait_timeout = jiffies + TIME_WAIT_INTERVAL;

			break;

		default:

			break;
		}

		break;
	}

	if(!skbuff_consumed)
		kfree_skb(skb);

	NbDebugPrint(3, ("lpx_stream_do_rcv end\n"));
	return 0;
}


/* Check lpx for retransmission, ConReqAck aware */
int lpx_stream_retransmit_chk(struct sock *sk, unsigned short ackseq, int type)
{
	struct lpxhdr	*lpxhdr;
	struct sk_buff  	*skb;
//	unsigned short		success_packets;

	UNREFERENCED_PARAMETER(type);

	skb = skb_peek(&LPX_STREAM_OPT(sk)->retransmit_queue);
	if(!skb)
		return 0;

	lpxhdr = (struct lpxhdr *)skb->nh.raw;
	if(((SHORT)(ackseq - NTOHS(lpxhdr->u.s.sequence))) <= 0)
		return 0;

//	success_packets = ackseq - NTOHS(lpxhdr->u.s.sequence);
//	flight_adaption(sk, success_packets);

	while((skb = skb_peek(&LPX_STREAM_OPT(sk)->retransmit_queue)) != NULL) {
		lpxhdr = (struct lpxhdr *)skb->nh.raw;
		if(((SHORT)(ackseq - NTOHS(lpxhdr->u.s.sequence))) > 0) {
			skb = skb_dequeue(&LPX_STREAM_OPT(sk)->retransmit_queue);
			kfree_skb(skb);
		} else
			break;
	}
			
	if(!skb_queue_empty(&LPX_STREAM_OPT(sk)->retransmit_queue)) {
//		LPX_STREAM_OPT(sk)->retransmit_timeout = jiffies + lpx_stream_calc_rtt(sk);
	} else {
		LPX_STREAM_OPT(sk)->interval_time = (LPX_STREAM_OPT(sk)->interval_time*99) + (jiffies - LPX_STREAM_OPT(sk)->latest_sendtime);
		if(LPX_STREAM_OPT(sk)->interval_time > 100)
			LPX_STREAM_OPT(sk)->interval_time /= 100;
		else
			LPX_STREAM_OPT(sk)->interval_time = 1;
		LPX_STREAM_OPT(sk)->latest_sendtime = 0;
	}
					

	if(LPX_STREAM_OPT(sk)->retransmits) {
		LPX_STREAM_OPT(sk)->retransmits = 0;
		LPX_STREAM_OPT(sk)->timer_reason |= SMP_RETRANSMIT_ACKED;
	} 
	else if (!(LPX_STREAM_OPT(sk)->timer_reason & SMP_RETRANSMIT_ACKED) 
		&& !(LPX_STREAM_OPT(sk)->timer_reason & SMP_SENDIBLE) 
		&& ((skb = skb_peek(&sk->write_queue)) != NULL) 
		&& lpx_stream_snd_test(sk, skb))
	{
		LPX_STREAM_OPT(sk)->timer_reason |= SMP_SENDIBLE;
	}	

	return 0;
}

#if 0

void flight_adaption (struct sock *sk, unsigned short success_packets)
{
	struct lpx_stream_opt		 *LPX_STREAM_OPT(sk) = (struct lpx_stream_opt *)(&sk->tp_pinfo.af_tcp);

	if(success_packets == 0) // when bug
		return; 

	if(LPX_STREAM_OPT(sk)->retransmits == 0 && LPX_STREAM_OPT(sk)->success_transmits == 0) { // while connection or bug
		NbDebugPrint(0, "LPX_STREAM_OPT(sk)->success_transmits = %d\n", LPX_STREAM_OPT(sk)->success_transmits);
		return;	
	}

	if(LPX_STREAM_OPT(sk)->retransmits) {
		LPX_STREAM_OPT(sk)->max_flights -= (LPX_STREAM_OPT(sk)->max_flights / LPX_STREAM_OPT(sk)->success_transmits); 
		if((SHORT)LPX_STREAM_OPT(sk)->max_flights < 10)
			LPX_STREAM_OPT(sk)->max_flights = 10; 
		LPX_STREAM_OPT(sk)->success_transmits = success_packets;
	} else if(((ULONG)LPX_STREAM_OPT(sk)->success_transmits + (ULONG)success_packets) <= 0xFFFF) {
		LPX_STREAM_OPT(sk)->success_transmits += success_packets;

		if((LPX_STREAM_OPT(sk)->max_flights*2 <= LPX_STREAM_OPT(sk)->success_transmits)
			&& (LPX_STREAM_OPT(sk)->max_flights < SMP_MAX_FLIGHT)) 
		{ 
			LPX_STREAM_OPT(sk)->max_flights ++;
		}
	}
}

#endif	

void lpx_stream_timer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
//void lpx_stream_timer(unsigned long data)
{
//	struct sock *sk = (struct sock*)data;
	struct sock *sk = (struct sock*)DeferredContext;

	struct sk_buff *skb, *skb2;
	struct lpxhdr *lpxhdr;
	struct sk_buff_head temp_queue;
	KIRQL	flags;
	
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	NbDebugPrint(1, ("called lpx_stream_timer, sk = %p, jiffies = %I64X, LPX_STREAM_OPT(sk)->alive_timeout = %I64X\n", 
		sk,
		jiffies, 
		LPX_STREAM_OPT(sk)->alive_timeout
		));
	
	NbDebugPrint(5, ("called lpx_stream_timer, sk = %p, sk->state = %d, sk->dead = %d\n", sk, sk->state, sk->dead));	
	
	NbDebugPrint(1, ("called lpx_stream_timer, sk = %p, jiffies = %I64X, LPX_STREAM_OPT(sk)->alive_timeout = %I64X, sk->state = %lx, sk->dead = %d\n", 
		sk,
		jiffies, 
		LPX_STREAM_OPT(sk)->alive_timeout, 
		sk->state, 
		sk->dead
		));	
	
	KeCancelTimer(&LPX_STREAM_OPT(sk)->IopTimer);

#ifdef __NDASBOOT__
	if( STATUS_NIC_OK != GetNICStatus() ) {
			
			NbDebugPrint(0, ("called lpx_stream_timer, NIC is corrupted\n"));

			sk->err = ETIMEDOUT;
			sk->error_report(sk);

			sk->shutdown = SHUTDOWN_MASK;
			sk->state = SMP_CLOSE;

			if(!sk->dead)
				sk->state_change(sk);

			sk->dead = 1;
			sk->destroy = 1;
			NbDebugPrint(0, ("called lpx_stream_timer return\n"));
			return;
	}
#endif

	if(sk->destroy) {
//		if(sk->user_data != NULL || LPX_STREAM_OPT(sk)->child_sklist) {
		if(LPX_STREAM_OPT(sk)->child_sklist) {
			NbDebugPrint(0, ("called lpx_stream_timer, before destroy, sk=%p, jiffies = %I64X\n", sk, jiffies));
			LPX_STREAM_OPT(sk)->deltaTime.QuadPart = - SMP_DESTROY_TIMEOUT;
			KeSetTimer(&LPX_STREAM_OPT(sk)->IopTimer, LPX_STREAM_OPT(sk)->deltaTime, &LpxTimerDpc);
		} else
			lpx_stream_destroy_sock(sk);
	
		return;
	}

	if(sk->sock_readers) { // process locking
		NbDebugPrint(1, ("called lpx_stream_timer, sk->sock_readers=%lx\n", sk->sock_readers));
//		if(LPX_STREAM_OPT(sk)->timer_reason & SMP_SENDIBLE)
//			LPX_STREAM_OPT(sk)->deltaTime.QuadPart = jiffies + 1;
//		else
			LPX_STREAM_OPT(sk)->deltaTime.QuadPart = - SMP_TIMEOUT;

		KeSetTimer(&LPX_STREAM_OPT(sk)->IopTimer, LPX_STREAM_OPT(sk)->deltaTime, &LpxTimerDpc);
		return;
	}

	if((sk->state == SMP_CLOSE && sk->dead) 
		|| (sk->state == SMP_TIME_WAIT && LPX_STREAM_OPT(sk)->time_wait_timeout <= jiffies)) 
	{
		NbDebugPrint(0, ("called lpx_stream_timer, before destroy\n"));
		sk->destroy = 1;

		sk->state = SMP_CLOSE;

		LPX_STREAM_OPT(sk)->deltaTime.QuadPart = - SMP_DESTROY_TIMEOUT;
		KeSetTimer(&LPX_STREAM_OPT(sk)->IopTimer, LPX_STREAM_OPT(sk)->deltaTime, &LpxTimerDpc);
		return;
	}

	if(sk->state == SMP_CLOSE || sk->state == SMP_LISTEN)
		goto out;

	if(!skb_queue_empty(&LPX_STREAM_OPT(sk)->retransmit_queue)
		&& LPX_STREAM_OPT(sk)->retransmit_timeout <= jiffies) 
	{		
		NbDebugPrint(0, ("lpx_stream_timer retransmit LPX_STREAM_OPT(sk)->retransmits = %d, LPX_STREAM_OPT(sk)->retransmit_timeout = %I64X, jiffies = %I64X\n", 
				LPX_STREAM_OPT(sk)->retransmits, LPX_STREAM_OPT(sk)->retransmit_timeout, jiffies));

		if( ((sk->state == SMP_SYN_SENT) && (LPX_STREAM_OPT(sk)->retransmits > MAX_CONNECT_COUNT))
			|| ((sk->state != SMP_SYN_SENT) && (LPX_STREAM_OPT(sk)->retransmits > MAX_RETRANSMIT_COUNT))
			) 
		{
#if(SMP_DEBUG)
			NbDebugPrint(1, ("lpx_stream_retransmit_timeout LPX_STREAM_OPT(sk)->retransmits = %d jiffies = %I64X\n", 
				LPX_STREAM_OPT(sk)->retransmits, jiffies));

			sk->err = ETIMEDOUT;
			sk->error_report(sk);

			sk->shutdown = SHUTDOWN_MASK;
			sk->state = SMP_CLOSE;

			if(!sk->dead)
				sk->state_change(sk);

#ifdef __NDASBOOT__
			sk->destroy = 1;
			return;
//			goto out;
#endif
#endif
		}

		// Need to leave skb on the queue, aye the fear
//		ACQUIRE_SOCKLOCK(sk,flags);
		skb = skb_peek(&LPX_STREAM_OPT(sk)->retransmit_queue);
		if(!skb) {
			NbDebugPrint(0, ("no skbi - is not stable\n"));
//			RELEASE_SOCKLOCK(sk,flags);
			goto out;
		}

		lpxhdr = (struct lpxhdr *)skb->nh.raw;
		lpxhdr->u.s.ackseq = HTONS(LPX_STREAM_OPT(sk)->rmt_seq);

		// 1.0, 1.1 conreq packet loss bug fix: Netdisk ignore multiple connection request from same host. 
        //        So if host lose first connection ack, host cannot connect the disk.
        //    Work around: connect with different source port number.

		if (lpxhdr->u.s.lsctl & HTONS(LSCTL_CONNREQ)) {
            unsigned short port;
            // find another port number
            port = lpx_get_free_port_num();
            // Change port
            lpx_unreg_bind(sk);
            LPX_OPT(sk)->source_addr.port = HTONS(port);
            lpx_reg_bind(sk);
            lpxhdr->source_port = LPX_OPT(sk)->source_addr.port;
            NbDebugPrint(2, ("Conreq fix: changing port number to %d", port));
        }
		//////////////////////////////////////////////
		//
		//

		if(skb_cloned(skb))
			skb2 = skb_copy(skb, 0);
		else
			skb2 = skb_clone(skb, 0);
		skb2->sk = skb->sk;
//		RELEASE_SOCKLOCK(sk,flags);

		LPX_STREAM_OPT(sk)->retransmits++;
		LPX_STREAM_OPT(sk)->retransmit_timeout = jiffies + lpx_stream_calc_rtt(sk);
		LPX_STREAM_OPT(sk)->last_retransmit_seq = NTOHS(lpxhdr->u.s.sequence);

		NbDebugPrint(1, ("lpx_stream_retransmit jiffies = %I64X\n", jiffies));
		NbDebugPrint(1, ("called lpx_stream_timer retransmit, LPX_STREAM_OPT(sk)->rmt_ack = %x\n", LPX_STREAM_OPT(sk)->rmt_ack));
		NbDebugPrint(1, ("LPX_STREAM_OPT(sk)->last_retransmit_seq = %x, LPX_STREAM_OPT(sk)->max_flights = %d, LPX_STREAM_OPT(sk)->retransmits = %d\n", 
			LPX_STREAM_OPT(sk)->last_retransmit_seq, LPX_STREAM_OPT(sk)->max_flights, LPX_STREAM_OPT(sk)->retransmits));
		lpx_stream_route_skb(sk, skb2, RETRAN);

	} else if(LPX_STREAM_OPT(sk)->timer_reason & SMP_RETRANSMIT_ACKED) 
	{
		NbDebugPrint(0, ("called lpx_stream_timer retransmit acked\n"));
		if(skb_queue_empty(&LPX_STREAM_OPT(sk)->retransmit_queue)) {
			LPX_STREAM_OPT(sk)->timer_reason &= ~SMP_RETRANSMIT_ACKED;
			goto send_packet;
		}
	
//		ACQUIRE_SOCKLOCK(sk,flags); 
		skb_queue_head_init(&temp_queue);

		while((skb = skb_peek(&LPX_STREAM_OPT(sk)->retransmit_queue)) != NULL) {
			skb = skb_dequeue(&LPX_STREAM_OPT(sk)->retransmit_queue);
			skb_queue_head(&temp_queue, skb);
			lpxhdr = (struct lpxhdr *)skb->nh.raw;
			lpxhdr->u.s.ackseq = HTONS(LPX_STREAM_OPT(sk)->rmt_seq);

			if(skb_cloned(skb))
				skb2 = skb_copy(skb, 0);
			else
				skb2 = skb_clone(skb, 0);
			skb2->sk = skb->sk;

//			RELEASE_SOCKLOCK(sk,flags);

			LPX_STREAM_OPT(sk)->retransmit_timeout = jiffies + lpx_stream_calc_rtt(sk);
			lpx_stream_route_skb(sk, skb2, RETRAN);
//			ACQUIRE_SOCKLOCK(sk,flags); 
		}

		while((skb = skb_dequeue(&temp_queue)) != NULL) {
			skb_queue_head(&LPX_STREAM_OPT(sk)->retransmit_queue, skb);
		}
			
		LPX_STREAM_OPT(sk)->timer_reason &= ~SMP_RETRANSMIT_ACKED;

//		RELEASE_SOCKLOCK(sk,flags);

	} else if(LPX_STREAM_OPT(sk)->timer_reason & SMP_SENDIBLE) 
	{
send_packet:
		NbDebugPrint(0, ("called lpx_stream_timer retransmit send packet\n"));
//		ACQUIRE_SOCKLOCK(sk,flags); 		
		while((skb = skb_peek(&sk->write_queue)) != NULL) {
			if(lpx_stream_snd_test(sk, skb)) {
				skb = skb_dequeue(&sk->write_queue);
				lpxhdr = (struct lpxhdr *)skb->nh.raw;
				lpxhdr->u.s.ackseq = HTONS(LPX_STREAM_OPT(sk)->rmt_seq);
//				RELEASE_SOCKLOCK(sk,flags);
				lpx_stream_route_skb(skb->sk, skb, DATA);
//				ACQUIRE_SOCKLOCK(sk,flags); 
			} else
				break;
		}
		LPX_STREAM_OPT(sk)->timer_reason &= ~SMP_SENDIBLE;
//		RELEASE_SOCKLOCK(sk,flags);

	} else if (LPX_STREAM_OPT(sk)->alive_timeout <= jiffies) // alive message
	{
		NbDebugPrint(0, ("lpx_stream_alive sk = %p, sk->dead = %d, jiffies = %I64X, alive_retries = %d\n", sk, sk->dead, jiffies, LPX_STREAM_OPT(sk)->alive_retries));
		if(LPX_STREAM_OPT(sk)->alive_retries > MAX_ALIVE_COUNT) {
			NbDebugPrint(0, ("lpx_stream_alive sk = %p, sk->dead = %d, jiffies = %I64X, alive_retries = %d\n", sk, sk->dead, jiffies, LPX_STREAM_OPT(sk)->alive_retries));

			sk->err = ETIMEDOUT;
			sk->error_report(sk);

			sk->shutdown = SHUTDOWN_MASK;
			sk->state = SMP_CLOSE;

			if(!sk->dead)
				sk->state_change(sk);

#ifdef __NDASBOOT__
			sk->destroy = 1;

			return;
//			goto out;
#endif
		}
		LPX_STREAM_OPT(sk)->alive_timeout = jiffies + ALIVE_INTERVAL;
		LPX_STREAM_OPT(sk)->alive_retries ++;
		lpx_stream_transmit(sk, NULL, ACKREQ, 0);		
	}
		
out:
	LPX_STREAM_OPT(sk)->deltaTime.QuadPart = - SMP_TIMEOUT;
	KeSetTimer(&LPX_STREAM_OPT(sk)->IopTimer, LPX_STREAM_OPT(sk)->deltaTime, &LpxTimerDpc);

	return;
}