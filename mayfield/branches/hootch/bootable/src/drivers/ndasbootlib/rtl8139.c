/* rtl8139.c: A RealTek RTL8129/8139 Fast Ethernet driver for Linux. */
/*
	Written 1997-1999 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.
    All other rights reserved.

	This driver is for boards based on the RTL8129 and RTL8139 PCI ethernet
	chips.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771

	Support and updates available at
	http://cesdis.gsfc.nasa.gov/linux/drivers/rtl8139.html

	Twister-tuning table provided by Kinston <shangh@realtek.com.tw>.
*/

#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include "ndasboot.h"
#include "linux2win.h"
#include "errno.h"
#include "pci.h"
#include "skbuff.h"
#include "netdevice.h"
#include "ether.h"
#include "lpx.h"
#include "sock.h"
#include "LsProto.h"
#include "nic.h"

static const char *version =
"rtl8139.c:v1.07 5/6/99 Donald Becker http://cesdis.gsfc.nasa.gov/linux/drivers/rtl8139.html\n";

/* A few user-configurable values. */
/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;
#define rtl8129_debug debug
static int rtl8129_debug = 0;

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC.  */
static int multicast_filter_limit = 32;

/* Used to pass the full-duplex flag, etc. */
#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* Size of the in-memory receive ring. */
#define RX_BUF_LEN_IDX	3			/* 0==8K, 1==16K, 2==32K, 3==64K */
#define RX_BUF_LEN (8192 << RX_BUF_LEN_IDX)
/* Size of the Tx bounce buffers -- must be at least (dev->mtu+14+4). */
#define TX_BUF_SIZE	1536

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH 256	/* In bytes, rounded down to 32 byte units. */

/* The following settings are log_2(bytes)-4:  0 == 16 bytes .. 6==1024. */

//#define RX_FIFO_THRESH	4		/* Rx buffer level before first PCI xfer.  */
//#define RX_DMA_BURST	4		/* Maximum PCI burst, '4' is 256 bytes */
//#define TX_DMA_BURST	4		/* Calculate as 16<<val. */

//#define RX_FIFO_THRESH	5		/* Rx buffer level before first PCI xfer.  */
//#define RX_DMA_BURST	4		/* Maximum PCI burst, '4' is 256 bytes */
//#define TX_DMA_BURST	6		/* Calculate as 16<<val. */

#define RX_FIFO_THRESH	7		/* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST	7		/* Maximum PCI burst, '4' is 256 bytes */
#define TX_DMA_BURST	6		/* Calculate as 16<<val. */


/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (4*HZ)


/* Kernel compatibility defines, some common to David Hind's PCMCIA package.
   This is only in the support-all-kernels source code. */

#define RUN_AT(x) (jiffies + (x))
#define PCI_SUPPORT_VER1
#define dev_free_skb(skb) dev_kfree_skb(skb);

/* The I/O extent. */
#define RTL8129_TOTAL_SIZE 0x80

/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the RealTek RTL8129, the RealTek Fast
Ethernet controllers for PCI.  This chip is used on a few clone boards.


II. Board-specific settings

PCI bus devices are configured by the system at boot time, so no jumpers
need to be set on the board.  The system BIOS will assign the
PCI INTA signal to a (preferably otherwise unused) system IRQ line.
Note: Kernel versions earlier than 1.3.73 do not support shared PCI
interrupt lines.

III. Driver operation

IIIa. Rx Ring buffers

The receive unit uses a single linear ring buffer rather than the more
common (and more efficient) descriptor-based architecture.  Incoming frames
are sequentially stored into the Rx region, and the host copies them into
skbuffs.

Comment: While it is theoretically possible to process many frames in place,
any delay in Rx processing would cause us to drop frames.  More importantly,
the Linux protocol stack is not designed to operate in this manner.

IIIb. Tx operation

The RTL8129 uses a fixed set of four Tx descriptors in register space.
In a stunningly bad design choice, Tx frames must be 32 bit aligned.  Linux
aligns the IP header on word boundaries, and 14 byte ethernet header means
that almost all frames will need to be copied to an alignment buffer.

IVb. References

http://www.realtek.com.tw/cn/cn.html
http://cesdis.gsfc.nasa.gov/linux/misc/NWay.html

IVc. Errata

*/


/* This table drives the PCI probe routines.  It's mostly boilerplate in all
   of the drivers, and will likely be provided by some future kernel.
   Note the matching code -- the first table entry matchs all 56** cards but
   second only the 1234 card.
*/
enum pci_flags_bit {
	PCI_USES_IO=1, PCI_USES_MEM=2, PCI_USES_MASTER=4,
	PCI_ADDR0=0x10<<0, PCI_ADDR1=0x10<<1, PCI_ADDR2=0x10<<2, PCI_ADDR3=0x10<<3,
};
struct pci_id_info {
	const char *name;
	USHORT	vendor_id, device_id, device_id_mask, flags;
	int io_size;
	struct device *(*probe1)(int pci_bus, int pci_devfn, struct device *dev,
							 long ioaddr, int irq, int chip_idx, int fnd_cnt);
};

static struct device * rtl8129_probe1(int pci_bus, int pci_devfn,
									  struct device *dev, long ioaddr,
									  int irq, int chp_idx, int fnd_cnt);

static struct pci_id_info pci_tbl[] =
{{ "RealTek RTL8129 Fast Ethernet",
   0x10ec, 0x8129, 0xffff, PCI_USES_IO|PCI_USES_MASTER, 0x80, rtl8129_probe1},
 { "RealTek RTL8139 Fast Ethernet",
   0x10ec, 0x8139, 0xffff, PCI_USES_IO|PCI_USES_MASTER, 0x80, rtl8129_probe1},
 { "SMC1211TX EZCard 10/100 (RealTek RTL8139)",
   0x1113, 0x1211, 0xffff, PCI_USES_IO|PCI_USES_MASTER, 0x80, rtl8129_probe1},
 { "Accton MPX5030 (RealTek RTL8139)",
   0x1113, 0x1211, 0xffff, PCI_USES_IO|PCI_USES_MASTER, 0x80, rtl8129_probe1},
 {0,},						/* 0 terminated list. */
};

/* The capability table matches the chip table above. */
enum {HAS_MII_XCVR=0x01, HAS_CHIP_XCVR=0x02, HAS_LNK_CHNG=0x04};
static int rtl_cap_tbl[] = {
	HAS_MII_XCVR, HAS_CHIP_XCVR|HAS_LNK_CHNG, HAS_CHIP_XCVR|HAS_LNK_CHNG,
};


/* The rest of these values should never change. */
#define NUM_TX_DESC	4			/* Number of Tx descriptor registers. */
//#define NUM_TX_DESC		2			/* Number of Tx descriptor registers. */

/* Symbolic offsets to registers. */
enum RTL8129_registers {
	MAC0=0,						/* Ethernet hardware address. */
	MAR0=8,						/* Multicast filter. */
	TxStatus0=0x10,				/* Transmit status (Four 32bit registers). */
	TxAddr0=0x20,				/* Tx descriptors (also four 32bit). */
	RxBuf=0x30, RxEarlyCnt=0x34, RxEarlyStatus=0x36,
	ChipCmd=0x37, RxBufPtr=0x38, RxBufAddr=0x3A,
	IntrMask=0x3C, IntrStatus=0x3E,
	TxConfig=0x40, RxConfig=0x44,
	Timer=0x48,					/* A general-purpose counter. */
	RxMissed=0x4C,				/* 24 bits valid, write clears. */
	Cfg9346=0x50, Config0=0x51, Config1=0x52,
	FlashReg=0x54, GPPinData=0x58, GPPinDir=0x59, MII_SMI=0x5A, HltClk=0x5B,
	MultiIntr=0x5C, TxSummary=0x60,
	MII_BMCR=0x62, MII_BMSR=0x64, NWayAdvert=0x66, NWayLPAR=0x68,
	NWayExpansion=0x6A,
	/* Undocumented registers, but required for proper operation. */
	FIFOTMS=0x70,	/* FIFO Test Mode Select */
	CSCR=0x74,	/* Chip Status and Configuration Register. */
	PARA78=0x78, PARA7c=0x7c,	/* Magic transceiver parameter register. */

	MediaStatus = 0x58, BasicModeCtrl = 0x62,
	Config5=0xD8,

};

enum ChipCmdBits {
	CmdReset=0x10, CmdRxEnb=0x08, CmdTxEnb=0x04, RxBufEmpty=0x01, };

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits {
	PCIErr=0x8000, PCSTimeout=0x4000,
	RxFIFOOver=0x40, RxUnderrun=0x20, RxOverflow=0x10,
	TxErr=0x08, TxOK=0x04, RxErr=0x02, RxOK=0x01,
};
enum TxStatusBits {
	TxHostOwns=0x2000, TxUnderrun=0x4000, TxStatOK=0x8000,
	TxOutOfWindow=0x20000000, TxAborted=0x40000000, TxCarrierLost=0x80000000,
};
enum RxStatusBits {
	RxMulticast=0x8000, RxPhysical=0x4000, RxBroadcast=0x2000,
	RxBadSymbol=0x0020, RxRunt=0x0010, RxTooLong=0x0008, RxCRCErr=0x0004,
	RxBadAlign=0x0002, RxStatusOK=0x0001,
};

/* Twister tuning parameters from RealTek.
   Completely undocumented, but required to tune bad links. */
enum CSCRBits {
	CSCR_LinkOKBit=0x0400, CSCR_LinkChangeBit=0x0800,
	CSCR_LinkStatusBits=0x0f000, CSCR_LinkDownOffCmd=0x003c0,
	CSCR_LinkDownCmd=0x0f3c0,
};

enum NegotiationBits {
	AutoNegotiationEnable = 0x1000, 
	AutoNegotiationRestart = 0x0200, 
	AutoNegoAbility10half = 0x21, 
	AutoNegoAbility10full = 0x41, 
	AutoNegoAbility100half = 0x81, 
	AutoNegoAbility100full = 0x101, 
};

#define PARA78_default	0x78fa8388
#define PARA7c_default  0xcb38de43
#define PARA7c_xxx		0xcb38de43
#define FIFOTMS_default	0x20

unsigned long param[4][4]={
	{0x0cb39de43,0x0cb39ce43,0x0fb38de03,0x0cb38de43},
	{0x0cb39de43,0x0cb39ce43,0x0cb39ce83,0x0cb39ce83},
	{0x0cb39de43,0x0cb39ce43,0x0cb39ce83,0x0cb39ce83},
	{0x0bb39de43,0x0bb39ce43,0x0bb39ce83,0x0bb39ce83}
};

struct rtl8129_private {
	char devname[8];			/* Used only for kernel debugging. */
	const char *product_name;
	struct device *next_module;
	int chip_id;
	int chip_revision;
	unsigned char pci_bus, pci_devfn;
	KTIMER timer;	/* Media selection timer. */
	unsigned int cur_rx;		/* Index into the Rx buffer of next Rx pkt. */
	unsigned int cur_tx, dirty_tx, tx_flag;
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct sk_buff* tx_skbuff[NUM_TX_DESC];
	unsigned char *tx_buf[NUM_TX_DESC];	/* Tx bounce buffers */
	unsigned char *rx_ring;
	unsigned char *tx_bufs;				/* Tx bounce buffer region. */
	char phys[4];						/* MII device addresses. */
	char twistie, twist_cnt;			/* Twister tune state. */
	unsigned int tx_full:1;				/* The Tx queue is full. */
	unsigned int full_duplex:1;			/* Full-duplex operation requested. */
	unsigned int duplex_lock:1;
	unsigned int default_port:4;		/* Last dev->if_port value. */
	unsigned int media2:4;				/* Secondary monitored media port. */
	unsigned int medialock:1;			/* Don't sense media type. */
	unsigned int mediasense:1;			/* Media sensing in progress. */

	unsigned int AutoNegoAbility;
};

static int rtl8129_open(struct device *dev);
static int read_eeprom(long ioaddr, int location);
static int mdio_read(struct device *dev, int phy_id, int location);
static void mdio_write(struct device *dev, int phy_id, int location, int val);
static void rtl8129_timer(unsigned long data);
static void rtl8129_tx_timeout(struct device *dev);
static void rtl8129_init_ring(struct device *dev);
static int rtl8129_start_xmit(struct sk_buff *skb, struct device *dev);
static int rtl8129_get_status(struct device *dev);
static int rtl8129_rx(struct device *dev);
//static void rtl8129_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
static void rtl8129_interrupt(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);
static int rtl8129_close(struct device *dev);
static int mii_ioctl(struct device *dev, struct ifreq *rq, int cmd);
static __inline ULONG ether_crc(int length, unsigned char *data);
static void set_rx_mode(struct device *dev);


/* A list of all installed RTL8129 devices, for removing the driver module. */
static struct device *root_rtl8129_dev = NULL;

#ifdef __NDASBOOT__
static int link_initialized = 0;
#endif

KDPC Rtl8129TimerDpc;
KTIMER Rtl8129Timer;

KDPC InterruptTimerDpc;
KTIMER InterruptTimer;

/* Ideally we would detect all network cards in slot order.  That would
   be best done a central PCI probe dispatch, which wouldn't work
   well when dynamically adding drivers.  So instead we detect just the
   Rtl81*9 cards in slot order. */

int rtl8139_probe2(struct device *dev)
{
	int cards_found = 0;
	unsigned char pci_index = 0;
	unsigned char pci_bus = 0, pci_device_fn = 0x90;
	int chip_idx = 0x1, irq = 0x5;
	long ioaddr = 0xe400;

	dev = rtl8129_probe1(pci_bus, pci_device_fn, dev, ioaddr, irq, chip_idx, cards_found);

	cards_found++;

	return cards_found ? 0 : -ENODEV;
}

int rtl8139_probe(struct device *dev)
{
	int cards_found = 0;
	unsigned char pci_index = 0;
	unsigned char pci_bus, pci_device_fn;

	if ( ! pcibios_present())
		return -ENODEV;

	NbDebugPrint(0, ("rtl8139_probe Entered\n"));

//	DbgPrint("rtl8139_probe Entered\n");
	
	for (; pci_index < 0xff; pci_index++) {
		USHORT vendor, device, pci_command, new_command;
		int chip_idx, irq;
		long ioaddr;

		if (pcibios_find_class (PCI_CLASS_NETWORK_ETHERNET << 8, pci_index,
								&pci_bus, &pci_device_fn))
			continue;
		pcibios_read_config_word(pci_bus, pci_device_fn,
								 PCI_VENDOR_ID, &vendor);
		pcibios_read_config_word(pci_bus, pci_device_fn,
								 PCI_DEVICE_ID, &device);

		for (chip_idx = 0; pci_tbl[chip_idx].vendor_id; chip_idx++)
			if (vendor == pci_tbl[chip_idx].vendor_id
				&& (device & pci_tbl[chip_idx].device_id_mask) ==
				pci_tbl[chip_idx].device_id)
				break;
		if (pci_tbl[chip_idx].vendor_id == 0) 		/* Compiled out! */
			continue;

activate:
		{
			struct pci_dev *pdev = pci_find_slot(pci_bus, pci_device_fn);
			
#ifndef __ENABLE_LOADER__
			pci_device_recovery(pdev);
#endif
		}

		{
#if defined(PCI_SUPPORT_VER2)
			struct pci_dev *pdev = pci_find_slot(pci_bus, pci_device_fn);
			ioaddr = pdev->base_address[0] & ~3;
			irq = pdev->irq;
#else
			ULONG pci_ioaddr;
			UCHAR pci_irq_line;
			pcibios_read_config_byte(pci_bus, pci_device_fn,
									 PCI_INTERRUPT_LINE, &pci_irq_line);
			pcibios_read_config_dword(pci_bus, pci_device_fn,
									  PCI_BASE_ADDRESS_0, &pci_ioaddr);
			ioaddr = pci_ioaddr & ~3;
			irq = pci_irq_line;
#endif
		}

		if (ioaddr == 0) {
			NbDebugPrint(0, (" A National SemiConductor network adapter has been found, "
					"however it has not been assigned an I/O address.\n"
					"  You may need t o power-cycle the machine for this "
					"device to work!\n"));
			goto activate;
			continue;
		}

//		if ((pci_tbl[chip_idx].flags & PCI_USES_IO) &&
//			check_region(ioaddr, pci_tbl[chip_idx].io_size))
//			continue;

		dev = pci_tbl[chip_idx].probe1(pci_bus, pci_device_fn, dev, ioaddr,
									   irq, chip_idx, cards_found);

		if (dev  && (pci_tbl[chip_idx].flags & PCI_COMMAND_MASTER)) {
			UCHAR pci_latency;
			pcibios_read_config_byte(pci_bus, pci_device_fn,
									 PCI_LATENCY_TIMER, &pci_latency);
			if (pci_latency < 32) {
				NbDebugPrint(1, ("  PCI latency timer (CFLT) is "
					   "unreasonably low at %d.  Setting to 64 clocks.\n",
					   pci_latency));
				pcibios_write_config_byte(pci_bus, pci_device_fn,
										  PCI_LATENCY_TIMER, 64);
			}
		}
		dev = 0;
		cards_found++;
	}

	NbDebugPrint(0, ("cards_found = %d\n", cards_found)) ;
	return cards_found ? 0 : -ENODEV;
}

static struct device *rtl8129_probe1(int pci_bus, int pci_devfn,
									 struct device *dev, long ioaddr,
									 int irq, int chip_idx, int found_cnt)
{
	static int did_version = 0;			/* Already printed version info. */
	struct rtl8129_private *tp;
	int i, option = found_cnt < MAX_UNITS ? options[found_cnt] : 0;

	NbDebugPrint(0, ("pci_bus = 0x%x, pci_devfn = 0x%x, ioaddr = 0x%x, irq = 0x%x, chip_idx = 0x%x, found_cnt = 0x%x\n",
		   pci_bus, pci_devfn, ioaddr, irq, chip_idx, found_cnt));	


//	DbgPrint("pci_bus = 0x%x, pci_devfn = 0x%x, ioaddr = 0x%x, irq = 0x%x, chip_idx = 0x%x, found_cnt = 0x%x\n",
//		   pci_bus, pci_devfn, ioaddr, irq, chip_idx, found_cnt);	

	if (rtl8129_debug > 0  &&  did_version++ == 0) {
		NbDebugPrint(1, ("%s", version));
	}

//	dev = init_etherdev(dev, 0);

	NbDebugPrint(0, ("%s: %s at %#lx, IRQ %d \n",
		   dev->name, pci_tbl[chip_idx].name, ioaddr, irq));

//	DbgPrint("%s: %s at %#lx, IRQ %d \n",
//		   dev->name, pci_tbl[chip_idx].name, ioaddr, irq);
	

	/* Bring the chip out of low-power mode. */
	outb(0x00, ioaddr + Config1);

	read_eeprom(ioaddr, 10) ;
	if (read_eeprom(ioaddr, 0) != 0xffff)
		for (i = 0; i < 3; i++)
			((USHORT *)(dev->dev_addr))[i] = (USHORT)read_eeprom(ioaddr, i + 7);
	else
		for (i = 0; i < 6; i++)
			dev->dev_addr[i] = inb(ioaddr + MAC0 + i);

	NbDebugPrint(0, ("%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
		dev->dev_addr[0], 
		dev->dev_addr[1], 
		dev->dev_addr[2], 
		dev->dev_addr[3], 
		dev->dev_addr[4], 
		dev->dev_addr[5] 
	));

//	DbgPrint("%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n",
//		dev->dev_addr[0], 
//		dev->dev_addr[1], 
//		dev->dev_addr[2], 
//		dev->dev_addr[3], 
//		dev->dev_addr[4], 
//		dev->dev_addr[5] 
//	);


	/* We do a request_region() to register /proc/ioports info. */
//	request_region(ioaddr, pci_tbl[chip_idx].io_size, dev->name);

	dev->base_addr = ioaddr;
	dev->irq = irq;

	/* Some data structures must be quadword aligned. */
	tp = kmalloc(sizeof(*tp), 0);
	memset(tp, 0, sizeof(*tp));
	dev->priv = tp;

	tp->next_module = root_rtl8129_dev;
	root_rtl8129_dev = dev;

	tp->chip_id = chip_idx;
	tp->pci_bus = (unsigned char) pci_bus;
	tp->pci_devfn = (unsigned char) pci_devfn;

	NbDebugPrint(0, ("chip_idx = %d, rtl_cap_tbl[chip_idx] = %lx\n", chip_idx, rtl_cap_tbl[chip_idx])) ;
//	DbgPrint("chip_idx = %d, rtl_cap_tbl[chip_idx] = %lx\n", chip_idx, rtl_cap_tbl[chip_idx]) ;
	
	/* Find the connected MII xcvrs.
	   Doing this in open() would allow detecting external xcvrs later, but
	   takes too much time. */
	if (rtl_cap_tbl[chip_idx] & HAS_MII_XCVR) {
		int phy, phy_idx;
		for (phy = 0, phy_idx = 0; phy < 32 && phy_idx < sizeof(tp->phys);
			 phy++) {
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff  && mii_status != 0x0000) {
				tp->phys[phy_idx++] = (unsigned char)phy;
				NbDebugPrint(1, ( "%s: MII transceiver found at address %d.\n",
					   dev->name, phy));
//				DbgPrint( "%s: MII transceiver found at address %d.\n",
//					   dev->name, phy);
			}
		}
		if (phy_idx == 0) {
			NbDebugPrint(1, ( "%s: No MII transceivers found!  Assuming SYM "
				   "transceiver.\n",
				   dev->name));
//			DbgPrint( "%s: No MII transceivers found!  Assuming SYM "
//				   "transceiver.\n",
//				   dev->name);
			tp->phys[0] = -1;
		}
	} else
		tp->phys[0] = 32;

	/* Put the chip into low-power mode. */
	outb(0xC0, ioaddr + Cfg9346);
	outb(0x03, ioaddr + Config1);
	outb('H', ioaddr + HltClk);		/* 'R' would leave the clock running. */

//	option = 0x20f;
	
	tp->AutoNegoAbility = option & 0xf;
	if(option > 0) {
		switch(tp->AutoNegoAbility) {
			case 1: outw(AutoNegoAbility10half, ioaddr + NWayAdvert); break;
			case 2: outw(AutoNegoAbility10full, ioaddr + NWayAdvert); break;
			case 4: outw(AutoNegoAbility100half, ioaddr + NWayAdvert); break;
			case 8: outw(AutoNegoAbility100full, ioaddr + NWayAdvert); break;
			default: break;
		}
		outw(AutoNegotiationEnable|AutoNegotiationRestart, ioaddr + BasicModeCtrl);
	}
	else {
		outw(AutoNegoAbility10half | AutoNegoAbility10full | AutoNegoAbility100half | AutoNegoAbility100full,
			ioaddr + NWayAdvert);
		outw(AutoNegotiationEnable|AutoNegotiationRestart, ioaddr + BasicModeCtrl);
	}

	/* The lower four bits are the media type. */
	if (option > 0) {
		tp->full_duplex = (option & 0x200) ? 1 : 0;
		tp->default_port = option & 15;
		if (tp->default_port)
			tp->medialock = 1;
	}

	NbDebugPrint(0, ( "Options = %lx\n", option));
//	DbgPrint( "Options = %lx\n", option);

	if (found_cnt < MAX_UNITS  &&  full_duplex[found_cnt] > 0)
		tp->full_duplex = full_duplex[found_cnt];

	if (tp->full_duplex) {
		NbDebugPrint(1, ( "%s: Media type forced to Full Duplex.\n", dev->name));
		mdio_write(dev, tp->phys[0], 4, 0x141);
		tp->duplex_lock = 1;
	}

	/* The Rtl8129-specific entries in the device structure. */
	dev->open = &rtl8129_open;
	dev->hard_start_xmit = &rtl8129_start_xmit;
	dev->stop = &rtl8129_close;
	dev->get_status = &rtl8129_get_status;
//	dev->set_multicast_list = &set_rx_mode;
//	dev->do_ioctl = &mii_ioctl;

	return dev;
}

/* Serial EEPROM section. */

/*  EEPROM_Ctrl bits. */
#define EE_SHIFT_CLK	0x04	/* EEPROM shift clock. */
#define EE_CS			0x08	/* EEPROM chip select. */
#define EE_DATA_WRITE	0x02	/* EEPROM chip data in. */
#define EE_WRITE_0		0x00
#define EE_WRITE_1		0x02
#define EE_DATA_READ	0x01	/* EEPROM chip data out. */
#define EE_ENB			(0x80 | EE_CS)

/* Delay between EEPROM clock transitions.
   No extra delay is needed with 33Mhz PCI, but 66Mhz may change this.
 */

void __inline eeprom_delay(long ee_addr) 
{
#ifdef __ENABLE_LOADER__
	inl(ee_addr);

	KeDelayExecution(1);
#else
	LARGE_INTEGER deltaTime;

	deltaTime.QuadPart = - INTERRUPT_TIME; 
	KeDelayExecutionThread(KernelMode, FALSE, &deltaTime);
#endif
}

/* The EEPROM commands include the alway-set leading bit. */
#define EE_WRITE_CMD	(5 << 6)
#define EE_READ_CMD		(6 << 6)
#define EE_ERASE_CMD	(7 << 6)

static int read_eeprom(long ioaddr, int location)
{
	int i;
	unsigned retval = 0;
	long ee_addr = ioaddr + Cfg9346;
	int read_cmd = location | EE_READ_CMD;

	outb(EE_ENB & ~EE_CS, ee_addr);
	outb(EE_ENB, ee_addr);

	/* Shift the read command bits out. */
	for (i = 10; i >= 0; i--) {
		int dataval = (read_cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(EE_ENB | dataval, ee_addr);
		eeprom_delay(ee_addr);
		outb(EE_ENB | dataval | EE_SHIFT_CLK, ee_addr);
		eeprom_delay(ee_addr);
	}
	outb(EE_ENB, ee_addr);
	eeprom_delay(ee_addr);

	for (i = 16; i > 0; i--) {
		outb(EE_ENB | EE_SHIFT_CLK, ee_addr);
		eeprom_delay(ee_addr);
		retval = (retval << 1) | ((inb(ee_addr) & EE_DATA_READ) ? 1 : 0);
		outb(EE_ENB, ee_addr);
		eeprom_delay(ee_addr);
	}

	/* Terminate the EEPROM access. */
	outb(~EE_CS, ee_addr);
	return retval;
}

/* MII serial management: mostly bogus for now. */
/* Read and write the MII management registers using software-generated
   serial MDIO protocol.
   The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
   met by back-to-back PCI I/O cycles, but we insert a delay to avoid
   "overclocking" issues. */
#define MDIO_DIR		0x80
#define MDIO_DATA_OUT	0x04
#define MDIO_DATA_IN	0x02
#define MDIO_CLK		0x01
#define MDIO_WRITE0 (MDIO_DIR)
#define MDIO_WRITE1 (MDIO_DIR | MDIO_DATA_OUT)

#define mdio_delay()	inb(mdio_addr)

static char mii_2_8139_map[8] = {MII_BMCR, MII_BMSR, 0, 0, NWayAdvert,
								 NWayLPAR, NWayExpansion, 0 };

/* Syncronize the MII management interface by shifting 32 one bits out. */
static void mdio_sync(long mdio_addr)
{
	int i;

	for (i = 32; i >= 0; i--) {
		outb(MDIO_WRITE1, mdio_addr);
		mdio_delay();
		outb(MDIO_WRITE1 | MDIO_CLK, mdio_addr);
		mdio_delay();
	}
	return;
}
static int mdio_read(struct device *dev, int phy_id, int location)
{
	long mdio_addr = dev->base_addr + MII_SMI;
	int mii_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	int retval = 0;
	int i;

	if (phy_id > 31) {	/* Really a 8139.  Use internal registers. */
		return location < 8 && mii_2_8139_map[location] ?
			inw(dev->base_addr + mii_2_8139_map[location]) : 0;
	}
	mdio_sync(mdio_addr);
	/* Shift the read command bits out. */
	for (i = 15; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_DATA_OUT : 0;

		outb(MDIO_DIR | dataval, mdio_addr);
		mdio_delay();
		outb(MDIO_DIR | dataval | MDIO_CLK, mdio_addr);
		mdio_delay();
	}

	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		outb(0, mdio_addr);
		mdio_delay();
		retval = (retval << 1) | ((inb(mdio_addr) & MDIO_DATA_IN) ? 1 : 0);
		outb(MDIO_CLK, mdio_addr);
		mdio_delay();
	}
	return (retval>>1) & 0xffff;
}

static void mdio_write(struct device *dev, int phy_id, int location, int value)
{
	long mdio_addr = dev->base_addr + MII_SMI;
	int mii_cmd = (0x5002 << 16) | (phy_id << 23) | (location<<18) | value;
	int i;

	if (phy_id > 31) {	/* Really a 8139.  Use internal registers. */
		if (location < 8  &&  mii_2_8139_map[location])
			outw(value, dev->base_addr + mii_2_8139_map[location]);
		return;
	}
	mdio_sync(mdio_addr);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval = (mii_cmd & (1 << i)) ? MDIO_WRITE1 : MDIO_WRITE0;
		outb(dataval, mdio_addr);
		mdio_delay();
		outb(dataval | MDIO_CLK, mdio_addr);
		mdio_delay();
	}
	/* Clear out extra bits. */
	for (i = 2; i > 0; i--) {
		outb(0, mdio_addr);
		mdio_delay();
		outb(MDIO_CLK, mdio_addr);
		mdio_delay();
	}
	return;
}



static int
rtl8129_open(struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int i;
	LARGE_INTEGER deltaTime;
	ULONG RetryCount;
	
	/* Soft reset the chip. */
	outb(CmdReset, ioaddr + ChipCmd);  

#ifdef __INTERRUPT__
	/////////////////////////////////////////////////////////////////////////
	nic_irq = dev->irq;
	/////////////////////////////////////////////////////////////////////////

	if (request_irq(dev->irq, &rtl8129_interrupt, SA_SHIRQ, dev->name, dev)) {		
		return -EAGAIN;
	}	
#else

#ifndef __ENABLE_LOADER__
/*
	for(i=0;i<HIGH_LEVEL;i++) {
		HalDisableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		(KIRQL)i
		);
	}
*/
		HalDisableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		(KIRQL)PASSIVE_LEVEL
		);
#endif

#endif

#ifdef __NDASBOOT__
	link_initialized = 0;
#endif

//	DbgPrint("tp->tx_bufs = %p, tp->rx_ring = %p\n", tp->tx_bufs, tp->rx_ring) ;
	if(!tp->tx_bufs) tp->tx_bufs = kmalloc(TX_BUF_SIZE * NUM_TX_DESC, 0);
	if(!tp->rx_ring) tp->rx_ring = kmalloc(RX_BUF_LEN + 16, 0);
//	DbgPrint("tp->tx_bufs = %p, tp->rx_ring = %p\n", tp->tx_bufs, tp->rx_ring) ;

	if (tp->tx_bufs == NULL ||  tp->rx_ring == NULL) {

#ifdef __INTERRUPT__
		free_irq(dev->irq, dev);
#endif

		if (tp->tx_bufs)
			kfree(tp->tx_bufs);
		if (rtl8129_debug > 0) {
			NbDebugPrint(1, ("%s: Couldn't allocate a %d byte receive ring.\n",
				   dev->name, RX_BUF_LEN));
		}
		return -ENOMEM;
	}
	rtl8129_init_ring(dev);

	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((inb(ioaddr + ChipCmd) & CmdReset) == 0)
			break;

	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + MAC0 + i);

	/* Must enable Tx/Rx before setting transfer thresholds! */
	outb(CmdRxEnb | CmdTxEnb, ioaddr + ChipCmd);
	outl((RX_FIFO_THRESH << 13) | (RX_BUF_LEN_IDX << 11) | (RX_DMA_BURST<<8),
		 ioaddr + RxConfig);
	outl((TX_DMA_BURST<<8)|0x03000000, ioaddr + TxConfig);
	tp->tx_flag = (TX_FIFO_THRESH<<11) & 0x003f0000;

	tp->full_duplex = tp->duplex_lock;
	if (tp->phys[0] >= 0  ||  (rtl_cap_tbl[tp->chip_id] & HAS_MII_XCVR)) {
		USHORT mii_reg5 = (USHORT) mdio_read(dev, tp->phys[0], 5);

		if (mii_reg5 == 0xffff)
			;					/* Not there */
		else if ((mii_reg5 & 0x0100) == 0x0100
				 || (mii_reg5 & 0x00C0) == 0x0040)
			tp->full_duplex = 1;
		if (rtl8129_debug > 1) {
			NbDebugPrint(0, ("%s: Setting %s%s-duplex based on"
				   " auto-negotiated partner ability %4.4x.\n", dev->name,
				   mii_reg5 == 0 ? "" :
				   (mii_reg5 & 0x0180) ? "100mbps " : "10mbps ",
				   tp->full_duplex ? "full" : "half", mii_reg5));
		}
	}

	outb(0xC0, ioaddr + Cfg9346);
	outb(tp->full_duplex ? 0x60 : 0x20, ioaddr + Config1);
	outb(0x00, ioaddr + Cfg9346);

	NbDebugPrint(0, ("virt_to_bus(tp->rx_ring) = %08X\n", virt_to_bus(tp->rx_ring)));
	outl(virt_to_bus(tp->rx_ring), ioaddr + RxBuf);

	/* Start the chip's Tx and Rx process. */
	outl(0, ioaddr + RxMissed);
	set_rx_mode(dev);

	outb(CmdRxEnb | CmdTxEnb, ioaddr + ChipCmd);

	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;
 
	/* Enable all known interrupts by setting the interrupt mask. */
	outw(PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver
		 | TxErr | TxOK | RxErr | RxOK, ioaddr + IntrMask);

	
	if (rtl8129_debug > 1) {
		NbDebugPrint(0, ("%s: rtl8129_open() ioaddr %#lx IRQ %d"
			   " GP Pins %2.2x %s-duplex.\n",
			   dev->name, ioaddr, dev->irq, inb(ioaddr + GPPinData),
			   tp->full_duplex ? "full" : "half"));
	}

#ifndef __INTERRUPT__

	/* Set the timer to switch to check for link beat and perhaps switch
	   to an alternate media type. */

//	KeInitializeDpc( &Rtl8129TimerDpc, rtl8129_timer, dev );
//	KeInitializeTimer( &Rtl8129Timer );
//	deltaTime.QuadPart = - (24 * HZ) / 10;    /* 2.4 sec. */
//  (VOID) KeSetTimer( &Rtl8129Timer, deltaTime, &Rtl8129TimerDpc );


	KeInitializeDpc( &InterruptTimerDpc, rtl8129_interrupt, dev );
	KeInitializeTimer( &InterruptTimer );

	deltaTime.QuadPart = - INTERRUPT_TIME;    /* 2.4 sec. */
	KeSetTimer( &InterruptTimer, deltaTime, &InterruptTimerDpc );

//	deltaTime.QuadPart = 0;    
//  KeSetTimerEx( &InterruptTimer, deltaTime, INTERRUPT_TIME, &InterruptTimerDpc );

#endif

#ifdef __NDASBOOT__
	RetryCount = 0 ;
	while(RetryCount < 10) {		
		deltaTime.QuadPart = - HZ / 2;
		KeDelayExecutionThread(KernelMode, FALSE, &deltaTime);
		RetryCount ++;		
		if(link_initialized) break;
	}	
	
	if(0 == link_initialized) {
		rtl8129_close(dev);
		return -EMLINK;
	}
#endif

	return 0;
}

static void rtl8129_timer(unsigned long data)
{
	struct device *dev = (struct device *)data;
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 60*HZ;
	int mii_reg5 = mdio_read(dev, tp->phys[0], 5);
	LARGE_INTEGER deltaTime;

	///////////////////////////////////////////
	KeCancelTimer(&Rtl8129Timer);
	///////////////////////////////////////////

//	DbgPrint("rtl8129_timer !!!!!\n\n\n") ;
	if (! tp->duplex_lock  &&  mii_reg5 != 0xffff) {
//		int duplex = (mii_reg5&0x0100) || (mii_reg5 & 0x01C0) == 0x0040;
		unsigned int duplex = (mii_reg5&0x0100) || (mii_reg5 & 0x01C0) == 0x0040;
		if (tp->full_duplex != duplex) {
			tp->full_duplex = duplex;
			NbDebugPrint(1, ( "%s: Setting %s-duplex based on MII #%d link"
				   " partner ability of %4.4x.\n", dev->name,
				   tp->full_duplex ? "full" : "half", tp->phys[0], mii_reg5));
			outb(0xC0, ioaddr + Cfg9346);
			outb(tp->full_duplex ? 0x60 : 0x20, ioaddr + Config1);
			outb(0x00, ioaddr + Cfg9346);
		}
	}
	/* Check for bogusness. */
	if (inw(ioaddr + IntrStatus) & (TxOK | RxOK)) {
		int status = inw(ioaddr + IntrStatus);
		if (status & (TxOK | RxOK)) {	/* Double check */
			NbDebugPrint(1, ( "%s: RTL8139 Interrupt line blocked, status %x.\n",
				   dev->name, status));
//			rtl8129_interrupt(dev->irq, dev, 0);
			rtl8129_interrupt(NULL, dev, 0, 0);
		}
	}
	if (dev->tbusy  &&  jiffies - dev->trans_start >= 2*TX_TIMEOUT)
		rtl8129_tx_timeout(dev);

#if 0
	if (tp->twistie) {
		unsigned int CSCRval = inw(ioaddr + CSCR);		/* Read link status. */
		if (tp->twistie == 1) {
			if (CSCRval & CSCR_LinkOKBit) {
				outw(CSCR_LinkDownOffCmd, ioaddr + CSCR);
				tp->twistie = 2;
				next_tick = HZ/10;
			} else {
				outw(CSCR_LinkDownCmd, ioaddr + CSCR);
				outl(FIFOTMS_default,ioaddr + FIFOTMS);
				outl(PARA78_default ,ioaddr + PARA78);
				outl(PARA7c_default ,ioaddr + PARA7c);
				tp->twistie = 0;
			}
		} else if (tp->twistie == 2) {
			int linkcase = (CSCRval & CSCR_LinkStatusBits) >> 12;
			int row;
			if (linkcase >= 0x7000) row = 3;
			else if (linkcase >= 0x3000) row = 2;
			else if (linkcase >= 0x1000) row = 1;
			else row = 0;
			tp->twistie = row + 3;
			outw(0,ioaddr+FIFOTMS);
			outl(param[row][0], ioaddr+PARA7c);
			tp->twist_cnt = 1;
		} else {
			outl(param[tp->twistie-3][tp->twist_cnt], ioaddr+PARA7c);
			if (++tp->twist_cnt < 4) {
				next_tick = HZ/10;
			} else if (tp->twistie-3 == 3) {
				if ((CSCRval & CSCR_LinkStatusBits) != 0x7000) {
					outl(PARA7c_xxx, ioaddr+PARA7c);
					next_tick = HZ/10; 		/* 100ms. */
					outl(FIFOTMS_default, ioaddr+FIFOTMS);
					outl(PARA78_default,  ioaddr+PARA78);
					outl(PARA7c_default,  ioaddr+PARA7c);
					tp->twistie = 3 + 3;
					outw(0,ioaddr+FIFOTMS);
					outl(param[3][0], ioaddr+PARA7c);
					tp->twist_cnt = 1;
				}
			}
		}
	}
#endif

	if (rtl8129_debug > 2) {
		if (rtl_cap_tbl[tp->chip_id] & HAS_MII_XCVR) {
			NbDebugPrint(1, ("%s: Media selection tick, GP pins %2.2x.\n",
				   dev->name, inb(ioaddr + GPPinData)));
		}
		else {
			NbDebugPrint(1, ("%s: Media selection tick, Link partner %4.4x.\n",
				   dev->name, inw(ioaddr + NWayLPAR)));
		}
		NbDebugPrint(1, ("%s:  Other registers are IntMask %4.4x IntStatus %4.4x"
			   " RxStatus %4.4x.\n",
			   dev->name, inw(ioaddr + IntrMask), inw(ioaddr + IntrStatus),
			   inl(ioaddr + RxEarlyStatus)));
		NbDebugPrint(1, ("%s:  Chip config %2.2x %2.2x.\n",
			   dev->name, inb(ioaddr + Config0), inb(ioaddr + Config1)));
	}

	deltaTime.QuadPart = - next_tick;
	KeSetTimer( &Rtl8129Timer, deltaTime, &Rtl8129TimerDpc );
}

static void rtl8129_tx_timeout(struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int mii_reg;
	int i;

	if (rtl8129_debug > 0) {

		NbDebugPrint(1, ( "%s: Transmit timeout, status %2.2x %4.4x "
			   "media %2.2x.\n",
			   dev->name, inb(ioaddr + ChipCmd), inw(ioaddr + IntrStatus),
			   inb(ioaddr + GPPinData)));


	}
	/* Disable interrupts by clearing the interrupt mask. */
	outw(0x0000, ioaddr + IntrMask);
	/* Emit info to figure out what went wrong. */
	NbDebugPrint(1, ("%s: Tx queue start entry %d  dirty entry %d.\n",
		   dev->name, tp->cur_tx, tp->dirty_tx));
	for (i = 0; i < NUM_TX_DESC; i++) {
		NbDebugPrint(1, ("%s:  Tx descriptor %d is %8.8x.%s\n",
			   dev->name, i, inl(ioaddr + TxStatus0 + i*4),
			   i == tp->dirty_tx % NUM_TX_DESC ? " (queue head)" : ""));
		
	}
	NbDebugPrint(1, ("%s: MII #%d registers are:", dev->name, tp->phys[0]));
	for (mii_reg = 0; mii_reg < 8; mii_reg++) {
		NbDebugPrint(1, (" %4.4x", mdio_read(dev, tp->phys[0], mii_reg)));
	}
	NbDebugPrint(1, (".\n"));

	/* Soft reset the chip. */
	outb(CmdReset, ioaddr + ChipCmd);
	/* Check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((inb(ioaddr + ChipCmd) & CmdReset) == 0)
			break;
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + MAC0 + i);

	outb(0x00, ioaddr + Cfg9346);
	tp->cur_rx = 0;
	/* Must enable Tx/Rx before setting transfer thresholds! */
	outb(CmdRxEnb | CmdTxEnb, ioaddr + ChipCmd);
	outl((RX_FIFO_THRESH << 13) | (RX_BUF_LEN_IDX << 11) | (RX_DMA_BURST<<8),
		 ioaddr + RxConfig);
	outl((TX_DMA_BURST<<8), ioaddr + TxConfig);
	set_rx_mode(dev);
	{							/* Save the unsent Tx packets. */
		struct sk_buff *saved_skb[NUM_TX_DESC], *skb;
		int j;
		for (j = 0; tp->cur_tx - tp->dirty_tx > 0 ; j++, tp->dirty_tx++)
			saved_skb[j] = tp->tx_skbuff[tp->dirty_tx % NUM_TX_DESC];
		tp->dirty_tx = tp->cur_tx = 0;

		for (i = 0; i < j; i++) {
			skb = tp->tx_skbuff[i] = saved_skb[i];
			if ((long)skb->data & 3) {		/* Must use alignment buffer. */
				memcpy(tp->tx_buf[i], skb->data, skb->len);
				outl(virt_to_bus(tp->tx_buf[i]), ioaddr + TxAddr0 + i*4);
			} else
				outl(virt_to_bus(skb->data), ioaddr + TxAddr0 + i*4);
			/* Note: the chip doesn't have auto-pad! */
			outl(tp->tx_flag | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN),
				 ioaddr + TxStatus0 + i*4);
		}
		tp->cur_tx = i;
		while (i < NUM_TX_DESC)
			tp->tx_skbuff[i++] = 0;
		if (tp->cur_tx - tp->dirty_tx < NUM_TX_DESC) {/* Typical path */
			dev->tbusy = 0;
			tp->tx_full = 0;
		} else {
			tp->tx_full = 1;
		}
	}

	dev->trans_start = jiffies;
//	tp->stats.tx_errors++;
	/* Enable all known interrupts by setting the interrupt mask. */
	outw(PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver
		 | TxErr | TxOK  | RxErr | RxOK, ioaddr + IntrMask);
	return;
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void
rtl8129_init_ring(struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	int i;

	tp->tx_full = 0;
	tp->cur_rx = 0;
	tp->dirty_tx = tp->cur_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++) {
		tp->tx_skbuff[i] = 0;
		tp->tx_buf[i] = &tp->tx_bufs[i*TX_BUF_SIZE];
	}
}

#define LPX_STREAM_OPT(sk) 		((struct lpx_stream_opt *)&sk->tp_pinfo.af_lpx_stream)

static int
rtl8129_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int entry;
	int status = 0;
	LARGE_INTEGER deltaTime;

	struct sock *sk = skb->sk;
	struct lpxhdr *lpxhdr = (struct lpxhdr *)skb->nh.raw;

	

//	KeCancelTimer(&InterruptTimer);

	if( STATUS_NIC_OK != rtl8129_get_status(dev) ) {
		return 1;
	}

//	NbDebugPrint(0, ("rtl8129_start_xmit: sk = %p, skb = %p, dev->tbusy = %d\n", sk, skb, dev->tbusy)) ; 
	
	
	/* Block a timer-based transmit from overlapping.  This could better be
	   done with atomic_swap(1, dev->tbusy), but set_bit() works as well. */
	if (dev->tbusy != 0) {
		if (jiffies - dev->trans_start >= TX_TIMEOUT)
			rtl8129_tx_timeout(dev);

		deltaTime.QuadPart = - INTERRUPT_TIME;    
//		KeSetTimer( &InterruptTimer, deltaTime, &InterruptTimerDpc );

		return 1;
	}

	dev->tbusy = 1;

	/* Calculate the next Tx descriptor entry. */
	entry = tp->cur_tx % NUM_TX_DESC;

//	NbDebugPrint(0, ("rtl8129_start_xmit: dev->interrupt = %d, skb = %p, skb->len = %d, entry = %02X\n", dev->interrupt, skb, skb->len, entry)) ; 

	tp->tx_skbuff[entry] = skb;
	if ((long)skb->data & 3) {			/* Must use alignment buffer. */
		memcpy(tp->tx_buf[entry], skb->data, skb->len);
		outl(virt_to_bus(tp->tx_buf[entry]), ioaddr + TxAddr0 + entry*4);
	} else
		outl(virt_to_bus(skb->data), ioaddr + TxAddr0 + entry*4);
	/* Note: the chip doesn't have auto-pad! */
	outl(tp->tx_flag | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN),
		 ioaddr + TxStatus0 + entry*4);

	if (++tp->cur_tx - tp->dirty_tx < NUM_TX_DESC) {	/* Typical path */
		clear_bit(0, (void*)&dev->tbusy);
	} else {
		tp->tx_full = 1;
	} 

//	if(sk) {
//		NbDebugPrint(0, ("X:lsctl = %04X, sk->sequence = %d, sk->rmt_seq = %d, sk->rmt_ack = %d, lpxhdr->sequence = %d, lpxhdr->ackseq = %d, jiffies = %lx\n",
//		NTOHS(lpxhdr->u.s.lsctl), LPX_STREAM_OPT(sk)->sequence, LPX_STREAM_OPT(sk)->rmt_seq, LPX_STREAM_OPT(sk)->rmt_ack, NTOHS(lpxhdr->u.s.sequence), NTOHS(lpxhdr->u.s.ackseq), jiffies));
//	}

	dev->trans_start = jiffies;
	if (rtl8129_debug > 4) {
		NbDebugPrint(1, ("%s: Queued Tx packet at %p size %d to slot %d.\n",
			   dev->name, skb->data, (int)skb->len, entry));
	}
	
	deltaTime.QuadPart = - TRANSMIT_TIME;    
//	KeSetTimer( &InterruptTimer, deltaTime, &InterruptTimerDpc );

//	wait_for_transmit(dev); 

	return 0;
}

static int
rtl8129_get_status(struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	long ioaddr = dev->base_addr;

	if( inl(ioaddr + RxBuf) != virt_to_bus(tp->rx_ring) ) {
		NbDebugPrint (0, ("rtl8129 NIC corrupted !!!!!!!\n"));	
		return STATUS_NIC_CORRUPTED;
	}

	return STATUS_NIC_OK;
}

void wait_for_transmit(struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	int boguscnt = max_interrupt_work;
	int status, link_changed = 0;
	long ioaddr = dev->base_addr;

	status = inw(ioaddr + IntrStatus);
	while((status & (TxOK | TxErr)) == 0) {
		status = inw(ioaddr + IntrStatus);		
	}


//	DbgPrint("wait_for_transmit\n");

	do {
		status = inw(ioaddr + IntrStatus);
		NbDebugPrint(1, ("dev->interrupt = %d, status = %lx\n", dev->interrupt, status)) ;
//		DbgPrint("dev->interrupt = %d, status = %lx\n", dev->interrupt, status) ;

		/* Acknowledge all of the current interrupt sources ASAP, but
		   an first get an additional status bit from CSCR. */
		if ((status & RxUnderrun)  &&  inw(ioaddr+CSCR) & CSCR_LinkChangeBit) {
			NbDebugPrint(0, ("Link changed: status = %lx\n", status)) ;

			link_changed = inw(ioaddr+CSCR) & CSCR_LinkChangeBit;

			if((inb(ioaddr+TxConfig+3) & 0x7C) == 0x74) {
				if((inb(ioaddr+GPPinData) & 0x04)==0)
					outb(inb(ioaddr+Config5) | 0x4, ioaddr+Config5);
				else
					outb(inb(ioaddr+Config5) | 0xb, ioaddr+Config5);
			}
		}
		outw(status, ioaddr + IntrStatus);

		if (rtl8129_debug > 4) {
			NbDebugPrint(0, ("%s: interrupt  status=%#4.4x new intstat=%#4.4x.\n",
				   dev->name, status, inw(ioaddr + IntrStatus)));
		}

		if ((status & (PCIErr|PCSTimeout|RxUnderrun|RxOverflow|RxFIFOOver
			|TxErr|TxOK|RxErr|RxOK)) == 0) {
			break;
		}		

		if (status & (TxOK | TxErr)) {
			unsigned int dirty_tx;
			int count = 0;

			for (dirty_tx = tp->dirty_tx; dirty_tx < tp->cur_tx; dirty_tx++) {
				int entry = dirty_tx % NUM_TX_DESC;
				int txstatus = inl(ioaddr + TxStatus0 + entry*4);				

				NbDebugPrint(1, ("%s: Transmit, intstat = %#4.4x, Tx status %8.8x.\n",
							   dev->name, status, txstatus));
				
				if ( ! (txstatus & (TxStatOK | TxUnderrun | TxAborted))) {
					NbDebugPrint(0, ("Don't Txed.\n"));	
					NbDebugPrint(0, ("%s: Transmit, entry = %d,  count = %d, intstat = %#4.4x, Tx status %8.8x.\n",
							   dev->name, entry, count, status, txstatus));
					break;			/* It still hasn't been Txed */
				}				
//				else {
//					NbDebugPrint(0, ("Txed !!!.\n"));
//					NbDebugPrint(0, ("%s: Transmit, entry = %d,  count = %d, intstat = %#4.4x, Tx status %8.8x.\n",
//							   dev->name, entry, count, status, txstatus));

//				}		

				count++;

				/* Note: TxCarrierLost is always asserted at 100mbps. */
				if (txstatus & (TxOutOfWindow | TxAborted)) {
					/* There was an major error, log it. */
					if (rtl8129_debug > 1) {
						NbDebugPrint(0, ("%s: Transmit error, Tx status %8.8x.\n",
							   dev->name, txstatus));
					}
//					tp->stats.tx_errors++;
					if (txstatus&TxAborted) {
//						tp->stats.tx_aborted_errors++;
						outl((TX_DMA_BURST<<8)|0x03000001, ioaddr + TxConfig);
					}
//					if (txstatus&TxCarrierLost) tp->stats.tx_carrier_errors++;
//					if (txstatus&TxOutOfWindow) tp->stats.tx_window_errors++;
				} else {
					if (txstatus & TxUnderrun) {
						/* Add 64 to the Tx FIFO threshold. */
						if (tp->tx_flag <  0x00300000)
							tp->tx_flag += 0x00020000;
//						tp->stats.tx_fifo_errors++;
					}
//					tp->stats.collisions += (txstatus >> 24) & 15;
				}
				
				/* Free the original skb. */

				dev_free_skb(tp->tx_skbuff[entry]);
				tp->tx_skbuff[entry] = 0;
				if (tp->tx_full) {
					/* The ring is no longer full, clear tbusy. */
					tp->tx_full = 0;
					clear_bit(0, (void*)&dev->tbusy);					
//					mark_bh(NET_BH);
				}
			}			
			
#ifndef final_version
			if (tp->cur_tx - dirty_tx > NUM_TX_DESC) {
				NbDebugPrint(0, ("%s: Out-of-sync dirty pointer, %d vs. %d, full=%d.\n",
					   dev->name, dirty_tx, tp->cur_tx, tp->tx_full));
				dirty_tx += NUM_TX_DESC;
			}
#endif
			tp->dirty_tx = dirty_tx;
		}

		/* Check uncommon events with one test. */
		if (status & (PCIErr|PCSTimeout |RxUnderrun|RxOverflow|RxFIFOOver
					  |TxErr|RxErr)) {
			if (rtl8129_debug > 2) {
				NbDebugPrint(0, ("%s: Abnormal interrupt, status %8.8x.\n",
					   dev->name, status));
			}

			if (status == 0xffffffff)
				break;
			/* Update the error count. */
//			tp->stats.rx_missed_errors += inl(ioaddr + RxMissed);
			outl(0, ioaddr + RxMissed);

			if ((status & RxUnderrun)  &&  link_changed  &&
				(rtl_cap_tbl[tp->chip_id] & HAS_LNK_CHNG)) {
				/* Really link-change on new chips. */
				int lpar = inw(ioaddr + NWayLPAR);
				unsigned int CSCRval = inw(ioaddr + CSCR);		/* Read link status. */
//				int duplex = (lpar&0x0100)||(lpar & 0x01C0) == 0x0040; 
				unsigned int duplex = (lpar&0x0100)||(lpar & 0x01C0) == 0x0040 || tp->duplex_lock; 

				NbDebugPrint(0, ( "link changed\n"));
				if (tp->full_duplex != duplex) {
					NbDebugPrint(0, ("tp->full_duplex = %d vs duplex = %d, Not Equal:%lx\n", tp->full_duplex, duplex, jiffies));
					tp->full_duplex = duplex;
					outb(0xC0, ioaddr + Cfg9346);
					outb(tp->full_duplex ? 0x60 : 0x20, ioaddr + Config1);
					outb(0x00, ioaddr + Cfg9346);
					//////////////////////////////
					if(CSCRval & CSCR_LinkOKBit) {
						struct sk_buff *skb;
						struct ethhdr *eth;
//						unsigned char data[32] = "this is a test packet";

						skb = alloc_skb(ETH_ZLEN, 0);

						skb_reserve(skb,ETH_HLEN);
						eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);
						memcpy(eth->h_dest,dev->broadcast, ETH_ALEN);
						memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
						eth->h_proto = 0;
						rtl8129_start_xmit(skb,dev);
					}

					//////////////////////////////
				}
#ifdef __NDASBOOT__
				link_initialized = 1;
#endif
				status &= ~RxUnderrun;
			}
//			if (status & (RxUnderrun | RxOverflow | RxErr | RxFIFOOver))
//				tp->stats.rx_errors++;

//			if (status & (PCSTimeout)) tp->stats.rx_length_errors++;
//			if (status & (RxUnderrun|RxFIFOOver)) tp->stats.rx_fifo_errors++;
			if (status & RxOverflow) {
//				tp->stats.rx_over_errors++;
				tp->cur_rx = inw(ioaddr + RxBufAddr) % RX_BUF_LEN;
				outw(tp->cur_rx - 16, ioaddr + RxBufPtr);
			}
			if (status & PCIErr) {
				ULONG pci_cmd_status;
				pcibios_read_config_dword(tp->pci_bus, tp->pci_devfn,
										  PCI_COMMAND, &pci_cmd_status);

				NbDebugPrint(0, ( "%s: PCI Bus error %4.4x.\n",
					   dev->name, pci_cmd_status));

				pcibios_write_config_word(0x00, 0xf0, PCI_COMMAND, 0x07);

				
				pcibios_read_config_dword(0x00, 0xf0,
										  PCI_COMMAND, &pci_cmd_status);

				NbDebugPrint(0, ( "%s: PCI Bridge Bus error %4.4x.\n",
					   dev->name, pci_cmd_status));
			}
		}
		if (--boguscnt < 0) {
			NbDebugPrint(0, ("%s: Too much work at interrupt, "
				   "IntrStatus=0x%4.4x.\n",
				   dev->name, status));
			/* Clear all interrupt sources. */
			outw(0xffff, ioaddr + IntrStatus);
			break;
		}
	} while(1) ;

	return ;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
//static void rtl8129_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
static void rtl8129_interrupt(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
	struct device *dev = (struct device *)DeferredContext;
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	int boguscnt = max_interrupt_work;
	int status, link_changed = 0;
	long ioaddr = dev->base_addr;
	LARGE_INTEGER deltaTime;

	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);
	
	NbDebugPrint(1, ("rtl8129_interrupt entered\n")) ;		

#ifndef __INTERRUPT__
	KeCancelTimer(&InterruptTimer);
#endif

	if( STATUS_NIC_OK != rtl8129_get_status(dev) ) {
#ifndef __INTERRUPT__

#ifndef __ENABLE_LOADER__
		HalEnableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		(KIRQL)PASSIVE_LEVEL,
		0
		);
#endif

#endif
		return;
	}	

#ifndef __INTERRUPT__
#ifndef __ENABLE_LOADER__
	HalDisableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		(KIRQL)PASSIVE_LEVEL
		);
#endif
#endif
/*
	DbgPrint("called rtl8129_interrupt, jiffies = %I64X, HZ = %I64X\n", 
		jiffies, 		 
		HZ
	);
*/

#if defined(__i386__)
	/* A lock to prevent simultaneous entry bug on Intel SMP machines. */
	if (test_and_set_bit(0, (void*)&dev->interrupt)) {
		NbDebugPrint(0, ("%s: SMP simultaneous entry of an interrupt handler.\n",
			   dev->name));
		dev->interrupt = 0;	/* Avoid halting machine. */
		
#ifndef __INTERRUPT__
		deltaTime.QuadPart = - INTERRUPT_TIME;    
		(VOID) KeSetTimer( &InterruptTimer, deltaTime, &InterruptTimerDpc );
#endif

		return;
	}
#else
	if (dev->interrupt) {
		NbDebugPrint(0, ("%s: Re-entering the interrupt handler.\n", dev->name));

#ifndef __INTERRUPT__
		deltaTime.QuadPart = - INTERRUPT_TIME;    
	    KeSetTimer( &InterruptTimer, deltaTime, &InterruptTimerDpc );
#endif

		return;
	}
	dev->interrupt = 1;
#endif

	do {
		status = inw(ioaddr + IntrStatus);		

		NbDebugPrint(1, ("dev->interrupt = %d, status = %lx\n", dev->interrupt, status)) ;

		/* Acknowledge all of the current interrupt sources ASAP, but
		   an first get an additional status bit from CSCR. */
		if ((status & RxUnderrun)  &&  inw(ioaddr+CSCR) & CSCR_LinkChangeBit) {
			NbDebugPrint(0, ("Link changed: status = %lx\n", status)) ;

			link_changed = inw(ioaddr+CSCR) & CSCR_LinkChangeBit;

			if((inb(ioaddr+TxConfig+3) & 0x7C) == 0x74) {
				if((inb(ioaddr+GPPinData) & 0x04)==0)
					outb(inb(ioaddr+Config5) | 0x4, ioaddr+Config5);
				else
					outb(inb(ioaddr+Config5) | 0xb, ioaddr+Config5);
			}
		}
		outw(status, ioaddr + IntrStatus);

		if (rtl8129_debug > 4) {
			NbDebugPrint(0, ("%s: interrupt  status=%#4.4x new intstat=%#4.4x.\n",
				   dev->name, status, inw(ioaddr + IntrStatus)));
		}

		if ((status & (PCIErr|PCSTimeout|RxUnderrun|RxOverflow|RxFIFOOver
			|TxErr|TxOK|RxErr|RxOK)) == 0) {
			break;
		}

		if (status & (RxOK|RxUnderrun|RxOverflow|RxFIFOOver))/* Rx interrupt */
			rtl8129_rx(dev);

		if (status & (TxOK | TxErr)) {
			unsigned int dirty_tx;
			int count = 0;

			for (dirty_tx = tp->dirty_tx; dirty_tx < tp->cur_tx; dirty_tx++) {
				int entry = dirty_tx % NUM_TX_DESC;
				int txstatus = inl(ioaddr + TxStatus0 + entry*4);				

				NbDebugPrint(1, ("%s: Transmit, intstat = %#4.4x, Tx status %8.8x.\n",
							   dev->name, status, txstatus));
				
				if ( ! (txstatus & (TxStatOK | TxUnderrun | TxAborted))) {
					NbDebugPrint(1, ("Don't Txed.\n"));	
					NbDebugPrint(1, ("%s: Transmit, entry = %d,  count = %d, intstat = %#4.4x, Tx status %8.8x.\n",
							   dev->name, entry, count, status, txstatus));
					break;			/* It still hasn't been Txed */
				}				
//				else {
//					NbDebugPrint(0, ("Txed !!!.\n"));
//					NbDebugPrint(0, ("%s: Transmit, entry = %d,  count = %d, intstat = %#4.4x, Tx status %8.8x.\n",
//							   dev->name, entry, count, status, txstatus));

//				}		

				count++;

				/* Note: TxCarrierLost is always asserted at 100mbps. */
				if (txstatus & (TxOutOfWindow | TxAborted)) {
					/* There was an major error, log it. */
					if (rtl8129_debug > 1) {
						NbDebugPrint(0, ("%s: Transmit error, Tx status %8.8x.\n",
							   dev->name, txstatus));
					}
//					tp->stats.tx_errors++;
					if (txstatus&TxAborted) {
//						tp->stats.tx_aborted_errors++;
						outl((TX_DMA_BURST<<8)|0x03000001, ioaddr + TxConfig);
					}
//					if (txstatus&TxCarrierLost) tp->stats.tx_carrier_errors++;
//					if (txstatus&TxOutOfWindow) tp->stats.tx_window_errors++;
				} else {
					if (txstatus & TxUnderrun) {
						/* Add 64 to the Tx FIFO threshold. */
						if (tp->tx_flag <  0x00300000)
							tp->tx_flag += 0x00020000;
//						tp->stats.tx_fifo_errors++;
					}
//					tp->stats.collisions += (txstatus >> 24) & 15;
				}
				
				/* Free the original skb. */

				dev_free_skb(tp->tx_skbuff[entry]);
				tp->tx_skbuff[entry] = 0;
				if (tp->tx_full) {
					/* The ring is no longer full, clear tbusy. */
					tp->tx_full = 0;
					clear_bit(0, (void*)&dev->tbusy);					
//					mark_bh(NET_BH);
				}
			}			
			
#ifndef final_version
			if (tp->cur_tx - dirty_tx > NUM_TX_DESC) {
				NbDebugPrint(0, ("%s: Out-of-sync dirty pointer, %d vs. %d, full=%d.\n",
					   dev->name, dirty_tx, tp->cur_tx, tp->tx_full));
				dirty_tx += NUM_TX_DESC;
			}
#endif
			tp->dirty_tx = dirty_tx;
		}

		
		/* Check uncommon events with one test. */
		if (status & (PCIErr|PCSTimeout |RxUnderrun|RxOverflow|RxFIFOOver
					  |TxErr|RxErr)) {
			if (rtl8129_debug > 2) {
				NbDebugPrint(0, ("%s: Abnormal interrupt, status %8.8x.\n",
					   dev->name, status));
			}

			if (status == 0xffffffff)
				break;
			/* Update the error count. */
//			tp->stats.rx_missed_errors += inl(ioaddr + RxMissed);
			outl(0, ioaddr + RxMissed);

			if ((status & RxUnderrun)  &&  link_changed  &&
				(rtl_cap_tbl[tp->chip_id] & HAS_LNK_CHNG)) {
				/* Really link-change on new chips. */
				int lpar = inw(ioaddr + NWayLPAR);
				unsigned int CSCRval = inw(ioaddr + CSCR);		/* Read link status. */
//				int duplex = (lpar&0x0100)||(lpar & 0x01C0) == 0x0040; 
				unsigned int duplex = (lpar&0x0100)||(lpar & 0x01C0) == 0x0040 || tp->duplex_lock; 

				NbDebugPrint(0, ( "link changed\n"));
				if (tp->full_duplex != duplex) {
					NbDebugPrint(0, ("tp->full_duplex = %d vs duplex = %d, Not Equal:%lx\n", tp->full_duplex, duplex, jiffies));
					tp->full_duplex = duplex;
					outb(0xC0, ioaddr + Cfg9346);
					outb(tp->full_duplex ? 0x60 : 0x20, ioaddr + Config1);
					outb(0x00, ioaddr + Cfg9346);
					//////////////////////////////
					if(CSCRval & CSCR_LinkOKBit) {
						struct sk_buff *skb;
						struct ethhdr *eth;
//						unsigned char data[32] = "this is a test packet";

						skb = alloc_skb(ETH_ZLEN, 0);

						skb_reserve(skb,ETH_HLEN);
						eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);
						memcpy(eth->h_dest,dev->broadcast, ETH_ALEN);
						memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
						eth->h_proto = 0;
						rtl8129_start_xmit(skb,dev);
					}

					//////////////////////////////
				}

				link_initialized = 1;
				status &= ~RxUnderrun;
			}
//			if (status & (RxUnderrun | RxOverflow | RxErr | RxFIFOOver))
//				tp->stats.rx_errors++;

//			if (status & (PCSTimeout)) tp->stats.rx_length_errors++;
//			if (status & (RxUnderrun|RxFIFOOver)) tp->stats.rx_fifo_errors++;
			if (status & RxOverflow) {
//				tp->stats.rx_over_errors++;
				tp->cur_rx = inw(ioaddr + RxBufAddr) % RX_BUF_LEN;
				outw(tp->cur_rx - 16, ioaddr + RxBufPtr);
			}
			if (status & PCIErr) {
				ULONG pci_cmd_status;
				pcibios_read_config_dword(tp->pci_bus, tp->pci_devfn,
										  PCI_COMMAND, &pci_cmd_status);

				NbDebugPrint(0, ( "%s: PCI Bus error %4.4x.\n",
					   dev->name, pci_cmd_status));
			}
		}

		if (--boguscnt < 0) {
			NbDebugPrint(0, ("%s: Too much work at interrupt, "
				   "IntrStatus=0x%4.4x.\n",
				   dev->name, status));
			/* Clear all interrupt sources. */
			outw(0xffff, ioaddr + IntrStatus);
			break;
		}
//		status = inw(ioaddr + IntrStatus);

//		NbDebugPrint(1, ("%s: while interrupt  status=%#4.4x new intstat=%#4.4x.\n",
//				   dev->name, status, inw(ioaddr + IntrStatus)));

	} while (1);

	if (rtl8129_debug > 3) {
		NbDebugPrint(0, ("%s: exiting interrupt, intr_status=%#4.4x.\n",
			   dev->name, inl(ioaddr + IntrStatus)));
	}

#if defined(__i386__)
	clear_bit(0, (void*)&dev->interrupt);
#else
	dev->interrupt = 0;
#endif

#ifdef __NDASBOOT__
	if(link_initialized) {
		dev_xmit_all(NULL);
	}
#endif

#ifndef __INTERRUPT__
	deltaTime.QuadPart = - INTERRUPT_TIME;    
    KeSetTimer( &InterruptTimer, deltaTime, &InterruptTimerDpc );
#endif

	return;
}

/* The data sheet doesn't describe the Rx ring at all, so I'm guessing at the
   field alignments and semantics. */
static int rtl8129_rx(struct device *dev)
{
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	long ioaddr = dev->base_addr;
	unsigned char *rx_ring = tp->rx_ring;
	USHORT cur_rx = (USHORT)tp->cur_rx;

	NbDebugPrint(2, ("rtl8129_rx\n"));
	
	if (rtl8129_debug > 4) {
		NbDebugPrint(1, ("%s: In rtl8129_rx(), current %4.4x BufAddr %4.4x,"
			   " free to %4.4x, Cmd %2.2x.\n",
			   dev->name, cur_rx, inw(ioaddr + RxBufAddr),
			   inw(ioaddr + RxBufPtr), inb(ioaddr + ChipCmd)));
	}

	while ((inb(ioaddr + ChipCmd) & 1) == 0) {
		int ring_offset = cur_rx % RX_BUF_LEN;
		ULONG rx_status = *(ULONG*)(rx_ring + ring_offset);
		int rx_size = rx_status >> 16;

		if (rtl8129_debug > 4) {
			int i;
			NbDebugPrint(1, ("%s:  rtl8129_rx() status %4.4x, size %4.4x, cur %4.4x.\n",
				   dev->name, rx_status, rx_size, cur_rx));
			NbDebugPrint(1, ("%s: Frame contents ", dev->name));
			for (i = 0; i < 8; i++) {
				NbDebugPrint(1, (" %2.2x", rx_ring[ring_offset + i]));
			}
			NbDebugPrint(1, (".\n"));
		}

		
		NbDebugPrint(2, ("%s:  rtl8129_rx() status %4.4x, size %4.4x, cur %4.4x.\n",
				   dev->name, rx_status, rx_size, cur_rx));
		{
			int i;
			for (i = 0; i < 20; i++) {
				NbDebugPrint(2, (" %2.2x", rx_ring[ring_offset + i]));
			}
			NbDebugPrint(2, (".\n"));
		}

		if (rx_status & RxTooLong) {
			if (rtl8129_debug > 0) {
				NbDebugPrint(1, ("%s: Oversized Ethernet frame, status %4.4x!\n",
					   dev->name, rx_status));
			}
//			tp->stats.rx_length_errors++;
		} else if (rx_status &
				   (RxBadSymbol|RxRunt|RxTooLong|RxCRCErr|RxBadAlign)) {
			if (rtl8129_debug > 1) {
				NbDebugPrint(1, ("%s: Ethernet frame had errors,"
					   " status %4.4x.\n", dev->name, rx_status));
			}
//			tp->stats.rx_errors++;
//			if (rx_status & (RxBadSymbol|RxBadAlign))
//				tp->stats.rx_frame_errors++;
//			if (rx_status & (RxRunt|RxTooLong)) tp->stats.rx_length_errors++;
//			if (rx_status & RxCRCErr) tp->stats.rx_crc_errors++;
			/* Reset the receiver, based on RealTek recommendation. (Bug?) */
			tp->cur_rx = 0;
			outb(CmdTxEnb, ioaddr + ChipCmd);
			outb(CmdRxEnb | CmdTxEnb, ioaddr + ChipCmd);
			outl((RX_FIFO_THRESH << 13) | (RX_BUF_LEN_IDX << 11) |
				 (RX_DMA_BURST<<8), ioaddr + RxConfig);
		} else {
			/* Malloc up new buffer, compatible with net-2e. */
			/* Omit the four octet CRC from the length. */
			struct sk_buff *skb;

			skb = dev_alloc_skb(rx_size + 2);
			if (skb == NULL) {				
				NbDebugPrint(1, ("%s: Memory squeeze, deferring packet.\n",
					   dev->name));
				/* We should check that some rx space is free.
				   If not, free one and mark stats->rx_dropped++. */
//				tp->stats.rx_dropped++;
				break;
			}
			skb->dev = dev;
			skb_reserve(skb, 2);	/* 16 byte align the IP fields. */
			if (ring_offset+rx_size+4 > RX_BUF_LEN) {
				int semi_count = RX_BUF_LEN - ring_offset - 4;
				memcpy(skb_put(skb, semi_count), &rx_ring[ring_offset + 4],
					   semi_count);
				memcpy(skb_put(skb, rx_size-semi_count), rx_ring,
					   rx_size-semi_count);
				if (rtl8129_debug > 4) {
					int i;
					NbDebugPrint(1, ("%s:  Frame wrap @%d",
						   dev->name, semi_count));
					for (i = 0; i < 16; i++) {
						NbDebugPrint(1, (" %2.2x", rx_ring[i]));
					}
					NbDebugPrint(1, (".\n"));
					memset(rx_ring, 0xcc, 16);
				}
			} else {
#if 1  /* USE_IP_COPYSUM */
				eth_copy_and_sum(skb, &rx_ring[ring_offset + 4],
								 rx_size, 0);
				skb_put(skb, rx_size);
#else
				memcpy(skb_put(skb, rx_size), &rx_ring[ring_offset + 4],
					   rx_size);
#endif
			}
			skb->protocol = eth_type_trans(skb, dev);

			netif_rx(skb);
		}

		cur_rx = (cur_rx + rx_size + 4 + 3) & ~3;
		outw(cur_rx - 16, ioaddr + RxBufPtr);
		outw(RxOverflow|RxFIFOOver, ioaddr + IntrStatus); ////////////////// Update /////////////////////
	}
	if (rtl8129_debug > 4) {
		NbDebugPrint(1, ("%s: Done rtl8129_rx(), current %4.4x BufAddr %4.4x,"
			   " free to %4.4x, Cmd %2.2x.\n",
			   dev->name, cur_rx, inw(ioaddr + RxBufAddr),
			   inw(ioaddr + RxBufPtr), inb(ioaddr + ChipCmd)));
	}
	tp->cur_rx = cur_rx;
	return 0;
}

static int
rtl8129_close(struct device *dev)
{
	long ioaddr = dev->base_addr;
	struct rtl8129_private *tp = (struct rtl8129_private *)dev->priv;
	int i;

	if(!dev->start) return 0;

	dev->start = 0;
	dev->tbusy = 1;

	if (rtl8129_debug > 1) {
		NbDebugPrint(1, ("%s: Shutting down ethercard, status was 0x%4.4x.\n",
			   dev->name, inw(ioaddr + IntrStatus)));
	}

	/* Disable interrupts by clearing the interrupt mask. */
	outw(0x0000, ioaddr + IntrMask);

	/* Stop the chip's Tx and Rx DMA processes. */
	outb(0x00, ioaddr + ChipCmd);

	/* Update the error counts. */
//	tp->stats.rx_missed_errors += inl(ioaddr + RxMissed);
	outl(0, ioaddr + RxMissed);

//	KeCancelTimer(&Rtl8129Timer);
#ifndef __INTERRUPT__
	KeCancelTimer(&InterruptTimer);
#endif

#ifdef __INTERRUPT__
	free_irq(dev->irq, dev);
#endif

	for (i = 0; i < NUM_TX_DESC; i++) {
		if (tp->tx_skbuff[i])
			dev_free_skb(tp->tx_skbuff[i]);
		tp->tx_skbuff[i] = 0;
	}
	
	if(tp->rx_ring)
		kfree(tp->rx_ring);
	tp->rx_ring = NULL;

	if(tp->tx_bufs)
		kfree(tp->tx_bufs);
	tp->rx_ring = NULL;

	/* Green! Put the chip in low-power mode. */
	outb(0xC0, ioaddr + Cfg9346);
	outb(0x03, ioaddr + Config1);
	outb('H', ioaddr + HltClk);		/* 'R' would leave the clock running. */

	return 0;
}

/* Set or clear the multicast filter for this adaptor.
   This routine is not state sensitive and need not be SMP locked. */

static unsigned const ethernet_polynomial = 0x04c11db7U;
static __inline ULONG ether_crc(int length, unsigned char *data)
{
    int crc = -1;

    while (--length >= 0) {
		unsigned char current_octet = *data++;
		int bit;
		for (bit = 0; bit < 8; bit++, current_octet >>= 1)
			crc = (crc << 1) ^
				((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
    }
    return crc;
}


/* Bits in RxConfig. */
enum rx_mode_bits {
	AcceptErr=0x20, AcceptRunt=0x10, AcceptBroadcast=0x08,
	AcceptMulticast=0x04, AcceptMyPhys=0x02, AcceptAllPhys=0x01,
};

static void set_rx_mode(struct device *dev)
{
	long ioaddr = dev->base_addr;
	ULONG mc_filter[2];		 /* Multicast hash filter */
	int i, rx_mode;

	if (rtl8129_debug > 3) {
		NbDebugPrint(1, ("%s:   set_rx_mode(%4.4x) done -- Rx config %8.8x.\n",
			   dev->name, dev->flags, inl(ioaddr + RxConfig)));
	}

	/* Note: do not reorder, GCC is clever about common statements. */
	if (dev->flags & IFF_PROMISC) {
		/* Unconditionally log net taps. */
		NbDebugPrint(1, ("%s: Promiscuous mode enabled.\n", dev->name));
		rx_mode = AcceptBroadcast|AcceptMulticast|AcceptMyPhys|AcceptAllPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else if ((dev->mc_count > multicast_filter_limit)
			   ||  (dev->flags & IFF_ALLMULTI)) {
		/* Too many to filter perfectly -- accept all multicasts. */
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0xffffffff;
	} else {
		struct dev_mc_list *mclist;
		rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
		mc_filter[1] = mc_filter[0] = 0;
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			 i++, mclist = mclist->next)
			set_bit(ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26, mc_filter);
	}
	/* We can safely update without stopping the chip. */
	outb(rx_mode, ioaddr + RxConfig);
	outl(mc_filter[0], ioaddr + MAR0 + 0);
	outl(mc_filter[1], ioaddr + MAR0 + 4);
	return;
}

/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -c rtl8139.c `[ -f /usr/include/linux/modversions.h ] && echo -DMODVERSIONS`"
 *  SMP-compile-command: "gcc -D__SMP__ -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -c rtl8139.c `[ -f /usr/include/linux/modversions.h ] && echo -DMODVERSIONS`"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
