#ifndef __ETHER_H
#define	__ETHER_H

#define	IP_PROTOCOL		0x0800
#define PSPX_AUTH_PORT	0x8000
#define PSPX_DATA_PORT	0x8010
#define ETH_P_802_2		0x0004
#define ETH_P_802_3		0x0001
#define ETH_P_IP		0x0800

#define ETH_P_LPX		0x88AD // LPX protocol

#define ETHER_ADDR_LEN		6
#define ETH_HLEN			14
#define ETH_ALEN			6
#define	MAX_ETHER_PACKET_SIZE	1518
#define MIN_ETHER_PACKET_SIZE	64

#define ETH_ZLEN		60
#define ETH_DATA_LEN    1500
#define IFF_PROMISC		0x100
#define IFF_ALLMULTI	0x200

/*
 *	This is an Ethernet frame header.
 */
 
struct ethhdr 
{
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	unsigned short	h_proto;		/* packet type ID field	*/
};

/*
 *	We Have changed the ethernet statistics collection data. This
 *	is just for partial compatibility for now.
 */
 
 
#define enet_statistics net_device_stats
void eth_copy_and_sum(struct sk_buff *dest, unsigned char *src, int length, int base);
unsigned short eth_type_trans(struct sk_buff *skb, struct device *dev);

void ether_setup(struct device *dev);
int eth_header(struct sk_buff *skb, struct device *dev, unsigned short type,
	   void *daddr, void *saddr, unsigned len);

#endif __ETHER_H
