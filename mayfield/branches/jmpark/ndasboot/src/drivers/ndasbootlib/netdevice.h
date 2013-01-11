/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Interfaces handler.
 *
 * Version:	@(#)dev.h	1.0.10	08/12/93
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Donald J. Becker, <becker@cesdis.gsfc.nasa.gov>
 *		Alan Cox, <Alan.Cox@linux.org>
 *		Bjorn Ekwall. <bj0rn@blox.se>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		Moved to /usr/include/linux for NET3
 */
#ifndef __NETDEVICE_H
#define __NETDEVICE_H

#include "bitops.h"

enum netdev_state_t
{
	__LINK_STATE_XOFF=0,
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_SCHED,
	__LINK_STATE_NOCARRIER,
	__LINK_STATE_RX_SCHED
};


/*
 *	For future expansion when we will have different priorities. 
 */
 
#define MAX_ADDR_LEN	7		/* Largest hardware address length */

/*
 *	Network device statistics. Akin to the 2.0 ether stats but
 *	with byte counters.
 */

struct net_device_stats
{
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received 	*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
	unsigned long	multicast;		/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;		/* receiver ring buff overflow	*/
	unsigned long	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;		/* recv'r fifo overrun		*/
	unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_window_errors;
	
	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};

/*
 *	We tag multicasts with these structures.
 */
 
struct dev_mc_list
{	
	struct dev_mc_list	*next;
	unsigned char		dmi_addr[MAX_ADDR_LEN];
	unsigned char		dmi_addrlen;
	int			dmi_users;
	int			dmi_gusers;
};

/*
 *	The DEVICE structure.
 *	Actually, this whole structure is a big mistake.  It mixes I/O
 *	data with strictly "high-level" data, and it has to know about
 *	almost every data structure used in the INET module.
 *
 *	FIXME: cleanup struct device such that network protocol info
 *	moves out.
 */

struct device
{
	/*
	 * This is the first field of the "visible" part of this structure
	 * (i.e. as seen by users in the "Space.c" file).  It is the name
	 * the interface.
	 */
	char			name[20];	
	/*
	 *	I/O specific fields
	 *	FIXME: Merge these and struct ifmap into one
	 */
	unsigned long		rmem_end;	/* shmem "recv" end	*/
	unsigned long		rmem_start;	/* shmem "recv" start	*/
	unsigned long		mem_end;	/* shared mem end	*/
	unsigned long		mem_start;	/* shared mem start	*/

	unsigned long		base_addr;	/* device I/O address	*/
	unsigned int		irq;		/* device IRQ number	*/

	/* Low-level status flags. */
	volatile unsigned char	start;		/* start an operation	*/
	/*
	 * These two are just single-bit flags, but due to atomicity
	 * reasons they have to be inside a "unsigned long". However,
	 * they should be inside the SAME unsigned long instead of
	 * this wasteful use of memory..
	 */
	unsigned long		interrupt;	/* bitops.. */
	unsigned long		tbusy;		/* transmitter busy */

	unsigned long		state;
	
	struct device		*next;
	
	/* The device initialization function. Called only once. */
	int			(*init)(struct device *dev);
	void		(*destructor)(struct device *dev);

	/* Interface index. Unique device identifier	*/
	int			ifindex;
	int			iflink;

	/*
	 *	Some hardware also needs these fields, but they are not
	 *	part of the usual set specified in Space.c.
	 */

	unsigned char		if_port;	/* Selectable AUI, TP,..*/
	unsigned char		dma;		/* DMA channel		*/

	struct net_device_stats*	(*get_stats)(struct device *dev);
	int	(*get_status)(struct device *dev);
	struct iw_statistics*	(*get_wireless_stats)(struct device *dev);

	/*
	 * This marks the end of the "visible" part of the structure. All
	 * fields hereafter are internal to the system, and may change at
	 * will (read: may be cleaned up at will).
	 */
	
	/* These may be needed for future network-power-down code. */
	LONGLONG			trans_start;	/* Time (in jiffies) of last Tx	*/
	LONGLONG			last_rx;	/* Time of last Rx	*/

	unsigned short		flags;	/* interface flags (a la BSD)	*/
	unsigned			mtu;	/* interface MTU value		*/

	unsigned short		hard_header_len;	/* hardware hdr length	*/

	void				*priv;	/* pointer to private data	*/
	
	/* Interface address info. */
	unsigned char		broadcast[MAX_ADDR_LEN];	/* hw bcast add	*/
//	unsigned char		pad;		/* make dev_addr aligned to 8 bytes */
	unsigned char		dev_addr[MAX_ADDR_LEN];	/* hw address	*/
	unsigned char		addr_len;	/* hardware address length	*/

	struct dev_mc_list	*mc_list;	/* Multicast mac addresses	*/
	int			mc_count;	/* Number of installed mcasts	*/
//	int			promiscuity;
//	int			allmulti;

	int			features;
#define NETIF_F_SG              1       /* Scatter/gather IO. */
#define NETIF_F_IP_CSUM         2       /* Can checksum only TCP/UDP over IPv4. */
#define NETIF_F_NO_CSUM         4       /* Does not require checksum. F.e. loopack. */
#define NETIF_F_HW_CSUM         8       /* Can checksum all the packets. */
#define NETIF_F_DYNALLOC        16      /* Self-dectructable device. */
#define NETIF_F_HIGHDMA         32      /* Can DMA to high memory. */
#define NETIF_F_FRAGLIST        64      /* Scatter/gather IO. */
#define NETIF_F_HW_VLAN_TX      128     /* Transmit VLAN hw acceleration */
#define NETIF_F_HW_VLAN_RX      256     /* Receive VLAN hw acceleration */
#define NETIF_F_HW_VLAN_FILTER  512     /* Receive filtering on VLAN */
#define NETIF_F_VLAN_CHALLENGED 1024    /* Device cannot handle VLAN packets */

	/* Pointers to interface service routines.	*/
	int			(*open)(struct device *dev);
	int			(*stop)(struct device *dev);
	int			(*hard_start_xmit) (struct sk_buff *skb,
						    struct device *dev);
	int			(*hard_header) (struct sk_buff *skb,
						struct device *dev,
						unsigned short type,
						void *daddr,
						void *saddr,
						unsigned len);
	int			(*rebuild_header)(struct sk_buff *skb);
	int			(*poll)(struct device *dev, int *budget);
	void		(*tx_timeout)(struct device *dev);
};


#define HAVE_NETIF_QUEUE

extern __inline void __netif_schedule(struct device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_SCHED, &dev->state)) {
//		unsigned long flags;
//		int cpu = smp_processor_id();

//		local_irq_save(flags);
//		dev->next_sched = softnet_data[cpu].output_queue;
//		softnet_data[cpu].output_queue = dev;
//		cpu_raise_softirq(cpu, NET_TX_SOFTIRQ);
//		local_irq_restore(flags);
	}
}

extern __inline void netif_schedule(struct device *dev)
{
	if (!test_bit(__LINK_STATE_XOFF, &dev->state))
		__netif_schedule(dev);
}

extern __inline void netif_start_queue(struct device *dev)
{
	clear_bit(__LINK_STATE_XOFF, &dev->state);
}

extern __inline void netif_wake_queue(struct device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_XOFF, &dev->state))
		__netif_schedule(dev);
}

extern __inline void netif_stop_queue(struct device *dev)
{
	set_bit(__LINK_STATE_XOFF, &dev->state);
}

extern __inline int netif_queue_stopped(struct device *dev)
{
	return test_bit(__LINK_STATE_XOFF, &dev->state);
}

extern __inline int netif_running(struct device *dev)
{
	return test_bit(__LINK_STATE_START, &dev->state);
}

/* Carrier loss detection, dial on demand. The functions netif_carrier_on
 * and _off may be called from IRQ context, but it is caller
 * who is responsible for serialization of these calls.
 */ 

extern __inline int netif_carrier_ok(struct device *dev)
{   
    return !test_bit(__LINK_STATE_NOCARRIER, &dev->state);
}                       

//extern void __netdev_watchdog_up(struct device *dev);
#define __netdev_watchdog_up

extern __inline void netif_carrier_on(struct device *dev)
{   
	clear_bit(__LINK_STATE_NOCARRIER, &dev->state);
	if (netif_running(dev))
		__netdev_watchdog_up(dev);
}   
    
extern __inline void netif_carrier_off(struct device *dev)
{   
    set_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

	/* Hot-plugging. */       
extern __inline int netif_device_present(struct device *dev)
{   
    return test_bit(__LINK_STATE_PRESENT, &dev->state);
}   

extern __inline void netif_device_detach(struct device *dev)
{
    if (test_and_clear_bit(__LINK_STATE_PRESENT, &dev->state) &&
        netif_running(dev)) {
        netif_stop_queue(dev);
    }
}

extern __inline void netif_device_attach(struct device *dev)
{
    if (!test_and_set_bit(__LINK_STATE_PRESENT, &dev->state) &&
        netif_running(dev)) {
        netif_wake_queue(dev);
        __netdev_watchdog_up(dev);
    }
}


/*
    * Network interface message level settings
*/
#define HAVE_NETIF_MSG 1

enum {
	NETIF_MSG_DRV       = 0x0001,
	NETIF_MSG_PROBE     = 0x0002,
	NETIF_MSG_LINK      = 0x0004,
	NETIF_MSG_TIMER     = 0x0008,
	NETIF_MSG_IFDOWN    = 0x0010,
	NETIF_MSG_IFUP      = 0x0020,
	NETIF_MSG_RX_ERR    = 0x0040,
	NETIF_MSG_TX_ERR    = 0x0080,
	NETIF_MSG_TX_QUEUED = 0x0100,
	NETIF_MSG_INTR      = 0x0200,
	NETIF_MSG_TX_DONE   = 0x0400,
	NETIF_MSG_RX_STATUS = 0x0800,
	NETIF_MSG_PKTDATA   = 0x1000,
	NETIF_MSG_HW        = 0x2000,
	NETIF_MSG_WOL       = 0x4000,
};

#define netif_msg_drv(p)    ((p)->msg_enable & NETIF_MSG_DRV)
#define netif_msg_probe(p)  ((p)->msg_enable & NETIF_MSG_PROBE)
#define netif_msg_link(p)   ((p)->msg_enable & NETIF_MSG_LINK)
#define netif_msg_timer(p)  ((p)->msg_enable & NETIF_MSG_TIMER)
#define netif_msg_ifdown(p) ((p)->msg_enable & NETIF_MSG_IFDOWN)
#define netif_msg_ifup(p)   ((p)->msg_enable & NETIF_MSG_IFUP)
#define netif_msg_rx_err(p) ((p)->msg_enable & NETIF_MSG_RX_ERR)
#define netif_msg_tx_err(p) ((p)->msg_enable & NETIF_MSG_TX_ERR)
#define netif_msg_tx_queued(p)  ((p)->msg_enable & NETIF_MSG_TX_QUEUED)
#define netif_msg_intr(p)   ((p)->msg_enable & NETIF_MSG_INTR)
#define netif_msg_tx_done(p)    ((p)->msg_enable & NETIF_MSG_TX_DONE)
#define netif_msg_rx_status(p)  ((p)->msg_enable & NETIF_MSG_RX_STATUS)
#define netif_msg_pktdata(p)    ((p)->msg_enable & NETIF_MSG_PKTDATA)
#define netif_msg_hw(p)     ((p)->msg_enable & NETIF_MSG_HW)
#define netif_msg_wol(p)    ((p)->msg_enable & NETIF_MSG_WOL)


extern int net_dev_init(void);
extern int net_dev_destroy(void);

extern int		dev_queue_xmit(struct sk_buff *skb, int);
extern void		dev_xmit_all(void *data);
#define HAVE_NETIF_RX 1
extern void		netif_rx(struct sk_buff *skb);

extern struct device NetDevice;

extern int rtl8139_probe(struct device *dev);
extern int tc59x_probe(struct device *dev);
extern int natsemi_probe(struct device *dev);
extern int skge_probe (struct device *dev);
extern int tg3_init_one(struct device *dev);

void wait_for_transmit(struct device *dev);

#endif	/* __NETDEVICE_H */
