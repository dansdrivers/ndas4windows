#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include <ntddk.h>

#include "ndasboot.h"
#include "debug.h"
#include "netdevice.h"
#include "ether.h"
#include "packet.h"
#include "skbuff.h"
#include "lpx.h"

struct sk_buff_head backlog;
struct sk_buff_head frontlog;

struct device NetDevice;

//#ifdef __ENABLE_LOADER__
unsigned int nic_irq = 0; 
//#endif

static __inline void ether_setup(struct device *dev);

int net_dev_init(void)
{
	int err = -1;

	skb_queue_head_init(&backlog);
	skb_queue_head_init(&frontlog);
	memset(&NetDevice, 0, sizeof(struct device));
	ether_setup(&NetDevice);
	
	do {
#if 0
		if(err) {		
			err = tg3_init_one(&NetDevice);		
			NbDebugPrint(0, ("tg3_init_one: err = %04X\n", err));
			if(!err) {
				err = NetDevice.open(&NetDevice);
			}
		}
#endif

#if 1
		if(err) {
			err = pcnet32_probe(&NetDevice);
			NbDebugPrint(0, ("pcnet32_probe() returned err = %04X\n", err));
			if(!err) {
				err = NetDevice.open(&NetDevice);
				break;
			}
		}
#endif
#if 1	
		if(err) {
			err = skge_probe(&NetDevice);
			NbDebugPrint(0, ("skge_probe: err = %04X\n", err));
			if(!err) {
				err = NetDevice.open(&NetDevice);
				break;
			}
		}
#endif
#if 1
		if(err) {
			err = natsemi_probe(&NetDevice);
			if(!err) {
				err = NetDevice.open(&NetDevice);
				break;
			}
		}
		if(err)	{
			err = rtl8139_probe(&NetDevice);		
			if(!err) {
				err = NetDevice.open(&NetDevice);
				break;
			}
		}
#endif

#if 0
		if(err)	{
			err = tc59x_probe(&NetDevice);
			if(!err) {
				err = NetDevice.open(&NetDevice);
				break;
			}
		}
#endif

	} while(FALSE);

	if(err) {
		NbDebugPrint(0, ("can't find NIC card !!!\n"));
		return -1;
	}	

	return 0;
}

int net_dev_destroy(void)
{
	int err = 0;

	if(NetDevice.get_status(&NetDevice) != STATUS_NIC_OK) {
		NbDebugPrint(0, ("Lost NIC. Cannot stop the NIC.\n"));
		return -1;
	}
	if(NetDevice.stop == NULL) {
		NbDebugPrint(0, ("Stop already performed.\n"));
		return -1;
	}

	err = NetDevice.stop(&NetDevice);
	NetDevice.stop = NULL;

	return err;
}

static __inline void ether_setup(struct device *dev)
{
	dev->hard_header	= eth_header;

	dev->hard_header_len 	= ETH_HLEN;
	dev->mtu		= 1500; /* eth_mtu */
	dev->addr_len		= ETH_ALEN;

	memset(dev->broadcast,0xFF, ETH_ALEN);

}

void netif_rx(struct sk_buff *skb)
{
	if(skb->protocol != HTONS(ETH_P_LPX)) {
		kfree_skb(skb) ;
	} else {
		skb->nh.raw = skb->data;
		lpxitf_rcv(skb, skb->dev, NULL);
	}

	return;
}

int eth_header(struct sk_buff *skb, struct device *dev, unsigned short type,
	   void *daddr, void *saddr, unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

	UNREFERENCED_PARAMETER(len);
	/* 
	 *	Set the protocol type. For a packet of type ETH_P_802_3 we put the length
	 *	in here instead. It is up to the 802.2 layer to carry protocol information.
	 */
	
	eth->h_proto = HTONS(type);

	/*
	 *	Set the source hardware address. 
	 */
	 
	if(saddr)
		memcpy(eth->h_dest,saddr,dev->addr_len);
	else
		memcpy(eth->h_source,dev->dev_addr,dev->addr_len);

	if(daddr)
	{
		memcpy(eth->h_dest,daddr,dev->addr_len);
	
		return dev->hard_header_len;
	}

	return -dev->hard_header_len;
}
 
unsigned short eth_type_trans(struct sk_buff *skb, struct device *dev)
{
	struct ethhdr *eth;
	unsigned char *rawp;
	
	skb->mac.raw=skb->data;
	skb_pull(skb,dev->hard_header_len);
	eth= skb->mac.ethernet;
	
	if(*eth->h_dest&1)
	{
		if(memcmp(eth->h_dest,dev->broadcast, ETH_ALEN)==0)
			skb->pkt_type=PACKET_BROADCAST;
		else
			skb->pkt_type=PACKET_MULTICAST;
	}
	
	/*
	 *	This ALLMULTI check should be redundant by 1.4
	 *	so don't forget to remove it.
	 *
	 *	Seems, you forgot to remove it. All silly devices
	 *	seems to set IFF_PROMISC.
	 */
	 
	else// if(1 /*dev->flags&IFF_PROMISC*/)
	{
		if(memcmp(eth->h_dest,dev->dev_addr, ETH_ALEN))
			skb->pkt_type=PACKET_OTHERHOST;
	}
	
	if (NTOHS(eth->h_proto) >= 1536)
		return eth->h_proto;
		
	rawp = skb->data;
	
	/*
	 *	This is a magic hack to spot IPX packets. Older Novell breaks
	 *	the protocol design and runs IPX over 802.3 without an 802.2 LLC
	 *	layer. We look for FFFF which isn't a used 802.2 SSAP/DSAP. This
	 *	won't work for fault tolerant netware but does for the rest.
	 */
	if (*(unsigned short *)rawp == 0xFFFF)
		return HTONS(ETH_P_802_3);
		
	/*
	 *	Real 802.2 LLC
	 */
	return HTONS(ETH_P_802_2);
}

void eth_copy_and_sum(struct sk_buff *dest, unsigned char *src, int length, int base)
{
	struct ethhdr *eth;

	UNREFERENCED_PARAMETER(base);

	eth=(struct ethhdr *)src;
	if(eth->h_proto!=HTONS(ETH_P_IP))
	{
		memcpy(dest->data,src,length);
		return;
	}
	/*
	 * We have to watch for padded packets. The csum doesn't include the
	 * padding, and there is no point in copying the padding anyway.
	 * We have to use the smaller of length and ip_length because it
	 * can happen that ip_length > length.
	 */
}

#ifdef __INTERRUPT__

int dev_queue_xmit(struct sk_buff *skb, int type)
{
	struct device	*dev;
	KIRQL	Irql;
	dev = skb->dev;			
	
//	KeAcquireSpinLock(&skb_queue_lock, &Irql); 	
	
	if(frontlog.qlen && type == ACK) {
		struct sk_buff *skb2 = skb_peek(&frontlog);
		struct lpxhdr *lpxhdr, *lpxhdr2;
	
		lpxhdr = (struct lpxhdr *)skb->nh.raw;

		do {			
			lpxhdr2 = (struct lpxhdr *)skb2->nh.raw;
			lpxhdr2->u.s.ackseq = lpxhdr->u.s.ackseq;
			skb2 = skb2->next;
		} while(skb2 != (struct sk_buff *)&frontlog);

		kfree_skb(skb);
	}
	else if(frontlog.qlen == 0 /*&& type != ACK*/ ) {
		if(dev->hard_start_xmit(skb, skb->dev) != 0) {			
				skb_queue_head(&frontlog, skb);				
			}	
	}
	else {
		skb_queue_tail(&frontlog, skb);
//		NbDebugPrint(0, ("dev_queue queued, sk = %p, Irql = %02X, CurIrql = %02X, frontlog.qlen = %02X, dev->interrupt = %d\n", skb->sk, Irql, KeGetCurrentIrql(), frontlog.qlen, dev->interrupt)) ;
	} 
	
//	KeReleaseSpinLock(&skb_queue_lock, Irql); 

	return 0;
}
#else
int dev_queue_xmit(struct sk_buff *skb, int type)
{
	struct device	*dev;
	KIRQL	Irql;
	dev = skb->dev;	

//	KeAcquireSpinLock(&skb_queue_lock, &Irql); 		

	if(frontlog.qlen && type == ACK) {
		struct sk_buff *skb2 = skb_peek(&frontlog);
		struct lpxhdr *lpxhdr, *lpxhdr2;
	
		lpxhdr = (struct lpxhdr *)skb->nh.raw;

		do {			
			lpxhdr2 = (struct lpxhdr *)skb2->nh.raw;
			lpxhdr2->u.s.ackseq = lpxhdr->u.s.ackseq;
			skb2 = skb2->next;
		} while(skb2 != (struct sk_buff *)&frontlog);

		kfree_skb(skb);
	}
	else if(frontlog.qlen == 0) {
		if(dev->hard_start_xmit(skb, skb->dev) != 0) {			
				skb_queue_head(&frontlog, skb);				
			}	
	}
	else {		
		skb_queue_tail(&frontlog, skb);
//		NbDebugPrint(0, ("dev_queue queued, sk = %p, Irql = %02X, CurIrql = %02X, frontlog.qlen = %02X, dev->interrupt = %d\n", skb->sk, Irql, KeGetCurrentIrql(), frontlog.qlen, dev->interrupt)) ;
	} 
	
//	KeReleaseSpinLock(&skb_queue_lock, Irql); 

	return 0;
}
#endif


void dev_xmit_all(void *data)
{
	struct device	*dev;
	struct sk_buff	*skb;
	KIRQL	Irql;

	data = data; 

//	KeAcquireSpinLock(&skb_queue_lock, &Irql); 	

//	NbDebugPrint(0, ("dev_xmit_all, frontlog.qlen = %02X\n", frontlog.qlen)) ;
	
	if(!skb_queue_empty(&frontlog)) {
		while(!skb_queue_empty(&frontlog)) {
			skb = skb_dequeue(&frontlog);
			dev = skb->dev;
//			NbDebugPrint(0, ("dev_xmit_all: dev->hard_start_xmit, frontlog.qlen = %02X\n", frontlog.qlen))
			if(dev->hard_start_xmit(skb, skb->dev) != 0) {			
				skb_queue_head(&frontlog, skb);
				break;
			}
		}
	}
	
//	KeReleaseSpinLock(&skb_queue_lock, Irql); 
}