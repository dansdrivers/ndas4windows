#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

/* EtherLinkXL.c: A 3Com EtherLink PCI III/XL ethernet driver for linux. */
/*
	Written 1996-1998 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

	This driver is for the 3Com "Vortex" and "Boomerang" series ethercards.
	Members of the series include Fast EtherLink 3c590/3c592/3c595/3c597
	and the EtherLink XL 3c900 and 3c905 cards.

	The author may be reached as becker@CESDIS.gsfc.nasa.gov, or C/O
	Center of Excellence in Space Data and Information Sciences
	   Code 930.5, Goddard Space Flight Center, Greenbelt MD 20771
*/

#include <ntddk.h>
#include <scsi.h>

#include "ndasboot.h"
#include "errno.h"
#include "time.h"
#include "pci.h"
#include "timer.h"
#include "skbuff.h"
#include "netdevice.h"
#include "ether.h"
#include "lpx.h"
#include "sock.h"
#include "LsProto.h"
#include "nic.h"

static char *version =
"3c59x.c:v0.99H 11/17/98 Donald Becker http://cesdis.gsfc.nasa.gov/linux/drivers/vortex.html\n";

/* "Knobs" that adjust features and parameters. */
/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1512 effectively disables this feature. */
static const int rx_copybreak = 200;
/* Allow setting MTU to a larger size, bypassing the normal ethernet setup. */
static const int mtu = 1500;
/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 20;

/* Put out somewhat more debugging messages. (0: no msg, 1 minimal .. 6). */
#define vortex_debug debug
#ifdef VORTEX_DEBUG
static int vortex_debug = VORTEX_DEBUG;
#else
static int vortex_debug = 0;
#endif

/* Some values here only for performance evaluation and path-coverage
   debugging. */
static int rx_nocopy = 0, rx_copy = 0, queued_packet = 0, rx_csumhits;

/* A few values that may be tweaked. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  ((400*HZ)/1000)

/* Keep the ring sizes a power of two for efficiency. */
#define TX_RING_SIZE	16
#define RX_RING_SIZE	32
#define PKT_BUF_SZ		1536			/* Size of each temporary Rx buffer.*/

/* Kernel compatibility defines, some common to David Hinds' PCMCIA package.
   This is only in the support-all-kernels source code. */

#define RUN_AT(x) (jiffies + (x))

//#include <linux/delay.h>

#define PCI_SUPPORT_VER2

#define DEV_FREE_SKB(skb) dev_kfree_skb(skb);

/* Operational parameter that usually are not changed. */

/* The Vortex size is twice that of the original EtherLinkIII series: the
   runtime register window, window 1, is now always mapped in.
   The Boomerang size is twice as large as the Vortex -- it has additional
   bus master control registers. */
#define VORTEX_TOTAL_SIZE 0x20
#define BOOMERANG_TOTAL_SIZE 0x40

/* Set iff a MII transceiver on any interface requires mdio preamble.
   This only set with the original DP83840 on older 3c905 boards, so the extra
   code size of a per-interface flag is not worthwhile. */
static char mii_preamble_required = 0;

/*
				Theory of Operation

I. Board Compatibility

This device driver is designed for the 3Com FastEtherLink and FastEtherLink
XL, 3Com's PCI to 10/100baseT adapters.  It also works with the 10Mbs
versions of the FastEtherLink cards.  The supported product IDs are
  3c590, 3c592, 3c595, 3c597, 3c900, 3c905

The related ISA 3c515 is supported with a separate driver, 3c515.c, included
with the kernel source or available from
    cesdis.gsfc.nasa.gov:/pub/linux/drivers/3c515.html

II. Board-specific settings

PCI bus devices are configured by the system at boot time, so no jumpers
need to be set on the board.  The system BIOS should be set to assign the
PCI INTA signal to an otherwise unused system IRQ line.

The EEPROM settings for media type and forced-full-duplex are observed.
The EEPROM media type should be left at the default "autoselect" unless using
10base2 or AUI connections which cannot be reliably detected.

III. Driver operation

The 3c59x series use an interface that's very similar to the previous 3c5x9
series.  The primary interface is two programmed-I/O FIFOs, with an
alternate single-contiguous-region bus-master transfer (see next).

The 3c900 "Boomerang" series uses a full-bus-master interface with separate
lists of transmit and receive descriptors, similar to the AMD LANCE/PCnet,
DEC Tulip and Intel Speedo3.  The first chip version retains a compatible
programmed-I/O interface that has been removed in 'B' and subsequent board
revisions.

One extension that is advertised in a very large font is that the adapters
are capable of being bus masters.  On the Vortex chip this capability was
only for a single contiguous region making it far less useful than the full
bus master capability.  There is a significant performance impact of taking
an extra interrupt or polling for the completion of each transfer, as well
as difficulty sharing the single transfer engine between the transmit and
receive threads.  Using DMA transfers is a win only with large blocks or
with the flawed versions of the Intel Orion motherboard PCI controller.

The Boomerang chip's full-bus-master interface is useful, and has the
currently-unused advantages over other similar chips that queued transmit
packets may be reordered and receive buffer groups are associated with a
single frame.

With full-bus-master support, this driver uses a "RX_COPYBREAK" scheme.
Rather than a fixed intermediate receive buffer, this scheme allocates
full-sized skbuffs as receive buffers.  The value RX_COPYBREAK is used as
the copying breakpoint: it is chosen to trade-off the memory wasted by
passing the full-sized skbuff to the queue layer for all frames vs. the
copying cost of copying a frame to a correctly-sized skbuff.


IIIC. Synchronization
The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and other software.

IV. Notes

Thanks to Cameron Spitzer and Terry Murphy of 3Com for providing development
3c590, 3c595, and 3c900 boards.
The name "Vortex" is the internal 3Com project name for the PCI ASIC, and
the EISA version is called "Demon".  According to Terry these names come
from rides at the local amusement park.

The new chips support both ethernet (1.5K) and FDDI (4.5K) packet sizes!
This driver only supports ethernet packets because of the skbuff allocation
limit of 4K.
*/

/* This table drives the PCI probe routines.  It's mostly boilerplate in all
   of the drivers, and will likely be provided by some future kernel.
*/

#define u8	UCHAR
#define u16	USHORT
#define u32 ULONG
#define s32 LONG

enum pci_flags_bit {
	PCI_USES_IO=1, PCI_USES_MEM=2, PCI_USES_MASTER=4,
	PCI_ADDR0=0x10<<0, PCI_ADDR1=0x10<<1, PCI_ADDR2=0x10<<2, PCI_ADDR3=0x10<<3,
};

struct pci_id_info {
	const char *name;
	u16	vendor_id, device_id, device_id_mask, flags;
	int drv_flags, io_size;
	struct device *(*probe1)(int pci_bus, int pci_devfn, struct device *dev,
							 long ioaddr, int irq, int chip_idx, int fnd_cnt);
};

enum { IS_VORTEX=1, IS_BOOMERANG=2, IS_CYCLONE=4,
	   HAS_PWR_CTRL=0x10, HAS_MII=0x20, HAS_NWAY=0x40, HAS_CB_FNS=0x80, };
static struct device *vortex_probe1(int pci_bus, int pci_devfn,
									struct device *dev, long ioaddr,
									int irq, int dev_id, int card_idx);
static struct pci_id_info pci_tbl[] = {
	{"3c590 Vortex 10Mbps",			0x10B7, 0x5900, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1},
	{"3c595 Vortex 100baseTx",		0x10B7, 0x5950, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1},
	{"3c595 Vortex 100baseT4",		0x10B7, 0x5951, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1},
	{"3c595 Vortex 100base-MII",	0x10B7, 0x5952, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_VORTEX, 32, vortex_probe1},
	{"3Com Vortex",					0x10B7, 0x5900, 0xff00,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1},
	{"3c900 Boomerang 10baseT",		0x10B7, 0x9000, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1},
	{"3c900 Boomerang 10Mbps Combo", 0x10B7, 0x9001, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1},
	{"3c900 Cyclone 10Mbps Combo", 0x10B7, 0x9005, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1},
	{"3c900B-FL Cyclone 10base-FL",	0x10B7, 0x900A, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1},
	{"3c905 Boomerang 100baseTx",	0x10B7, 0x9050, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG|HAS_MII, 64, vortex_probe1},
	{"3c905 Boomerang 100baseT4",	0x10B7, 0x9051, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG|HAS_MII, 64, vortex_probe1},
	{"3c905B Cyclone 100baseTx",	0x10B7, 0x9055, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_CYCLONE|HAS_NWAY, 128, vortex_probe1},
	{"3c905B-FX Cyclone 100baseFx",	0x10B7, 0x905A, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1},
	{"3c980 Cyclone",	0x10B7, 0x9800, 0xfff0,
	 PCI_USES_IO|PCI_USES_MASTER, IS_CYCLONE, 128, vortex_probe1},
	{"3c575 Boomerang CardBus",		0x10B7, 0x5057, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG|HAS_MII, 64, vortex_probe1},
	{"3CCFE575 Cyclone CardBus",	0x10B7, 0x5157, 0xffff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_CYCLONE|HAS_NWAY|HAS_CB_FNS,
	 128, vortex_probe1},
	{"3c575 series CardBus (unknown version)", 0x10B7, 0x5057, 0xf0ff,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG|HAS_MII, 64, vortex_probe1},
	{"3Com Boomerang (unknown version)",	0x10B7, 0x9000, 0xff00,
	 PCI_USES_IO|PCI_USES_MASTER, IS_BOOMERANG, 64, vortex_probe1},
	{0,},						/* 0 terminated list. */
};

/* Operational definitions.
   These are not used by other compilation units and thus are not
   exported in a ".h" file.

   First the windows.  There are eight register windows, with the command
   and status registers available in each.
   */
#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)
#define EL3_CMD 0x0e
#define EL3_STATUS 0x0e

/* The top five bits written to EL3_CMD are a command, the lower
   11 bits are the parameter, if applicable.
   Note that 11 parameters bits was fine for ethernet, but the new chip
   can handle FDDI length frames (~4500 octets) and now parameters count
   32-bit 'Dwords' rather than octets. */

enum vortex_cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11,
	UpStall = 6<<11, UpUnstall = (6<<11)+1,
	DownStall = (6<<11)+2, DownUnstall = (6<<11)+3,
	RxDiscard = 8<<11, TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
	SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11,
	StartDMAUp = 20<<11, StartDMADown = (20<<11)+1, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11, SetFilterBit = 25<<11,};

/* The SetRxFilter command accepts the following classes: */
enum RxFilter {
	RxStation = 1, RxMulticast = 2, RxBroadcast = 4, RxProm = 8 };

/* Bits in the general status register. */
enum vortex_status {
	IntLatch = 0x0001, HostError = 0x0002, TxComplete = 0x0004,
	TxAvailable = 0x0008, RxComplete = 0x0010, RxEarly = 0x0020,
	IntReq = 0x0040, StatsFull = 0x0080,
	DMADone = 1<<8, DownComplete = 1<<9, UpComplete = 1<<10,
	DMAInProgress = 1<<11,			/* DMA controller is still busy.*/
	CmdInProgress = 1<<12,			/* EL3_CMD is still busy.*/
};

/* Register window 1 offsets, the window used in normal operation.
   On the Vortex this window is always mapped at offsets 0x10-0x1f. */
enum Window1 {
	TX_FIFO = 0x10,  RX_FIFO = 0x10,  RxErrors = 0x14,
	RxStatus = 0x18,  Timer=0x1A, TxStatus = 0x1B,
	TxFree = 0x1C, /* Remaining free bytes in Tx buffer. */
};
enum Window0 {
	Wn0EepromCmd = 10,		/* Window 0: EEPROM command register. */
	Wn0EepromData = 12,		/* Window 0: EEPROM results register. */
	IntrStatus=0x0E,		/* Valid in all windows. */
};
enum Win0_EEPROM_bits {
	EEPROM_Read = 0x80, EEPROM_WRITE = 0x40, EEPROM_ERASE = 0xC0,
	EEPROM_EWENB = 0x30,		/* Enable erasing/writing for 10 msec. */
	EEPROM_EWDIS = 0x00,		/* Disable EWENB before 10 msec timeout. */
};
/* EEPROM locations. */
enum eeprom_offset {
	PhysAddr01=0, PhysAddr23=1, PhysAddr45=2, ModelID=3,
	EtherLink3ID=7, IFXcvrIO=8, IRQLine=9,
	NodeAddr01=10, NodeAddr23=11, NodeAddr45=12,
	DriverTune=13, Checksum=15};

enum Window2 {			/* Window 2. */
	Wn2_ResetOptions=12,
};
enum Window3 {			/* Window 3: MAC/config bits. */
	Wn3_Config=0, Wn3_MAC_Ctrl=6, Wn3_Options=8,
};
union wn3_config {
	int i;
	struct w3_config_fields {
		unsigned int ram_size:3, ram_width:1, ram_speed:2, rom_size:2;
		int pad8:8;
		unsigned int ram_split:2, pad18:2, xcvr:4, autoselect:1;
		int pad24:7;
	} u;
};

enum Window4 {		/* Window 4: Xcvr/media bits. */
	Wn4_FIFODiag = 4, Wn4_NetDiag = 6, Wn4_PhysicalMgmt=8, Wn4_Media = 10,
};
enum Win4_Media_bits {
	Media_SQE = 0x0008,		/* Enable SQE error counting for AUI. */
	Media_10TP = 0x00C0,	/* Enable link beat and jabber for 10baseT. */
	Media_Lnk = 0x0080,		/* Enable just link beat for 100TX/100FX. */
	Media_LnkBeat = 0x0800,
};
enum Window7 {					/* Window 7: Bus Master control. */
	Wn7_MasterAddr = 0, Wn7_MasterLen = 6, Wn7_MasterStatus = 12,
};
/* Boomerang bus master control registers. */
enum MasterCtrl {
	PktStatus = 0x20, DownListPtr = 0x24, FragAddr = 0x28, FragLen = 0x2c,
	TxFreeThreshold = 0x2f, UpPktStatus = 0x30, UpListPtr = 0x38,
};

/* The Rx and Tx descriptor lists.
   Caution Alpha hackers: these types are 32 bits!  Note also the 8 byte
   alignment contraint on tx_ring[] and rx_ring[]. */
#define LAST_FRAG  0x80000000			/* Last Addr/Len pair in descriptor. */
struct boom_rx_desc {
	u32 next;					/* Last entry points to 0.   */
	s32 status;
	u32 addr;					/* Up to 63 addr/len pairs possible. */
	s32 length;					/* Set LAST_FRAG to indicate last pair. */
};
/* Values for the Rx status entry. */
enum rx_desc_status {
	RxDComplete=0x00008000, RxDError=0x4000,
	/* See boomerang_rx() for actual error bits */
	IPChksumErr=1<<25, TCPChksumErr=1<<26, UDPChksumErr=1<<27,
	IPChksumValid=1<<29, TCPChksumValid=1<<30, UDPChksumValid=1<<31,
};

struct boom_tx_desc {
	u32 next;					/* Last entry points to 0.   */
	s32 status;					/* bits 0:12 length, others see below.  */
	u32 addr;
	s32 length;
};

/* Values for the Tx status entry. */
enum tx_desc_status {
	CRCDisable=0x2000, TxDComplete=0x8000,
	AddIPChksum=0x02000000, AddTCPChksum=0x04000000, AddUDPChksum=0x08000000,
	TxIntrUploaded=0x80000000,		/* IRQ when in FIFO, but maybe not sent. */
};

/* Chip features we care about in vp->capabilities, read from the EEPROM. */
enum ChipCaps { CapBusMaster=0x20 };

struct vortex_private {
	/* The Rx and Tx rings should be quad-word-aligned. */
	struct boom_rx_desc rx_ring[RX_RING_SIZE];
	struct boom_tx_desc tx_ring[TX_RING_SIZE];
	/* The addresses of transmit- and receive-in-place skbuffs. */
	struct sk_buff* rx_skbuff[RX_RING_SIZE];
	struct sk_buff* tx_skbuff[TX_RING_SIZE];
	struct device *next_module;
	void *priv_addr;
	unsigned int cur_rx, cur_tx;		/* The next free ring entry */
	unsigned int dirty_rx, dirty_tx;	/* The ring entries to be free()ed. */
	struct net_device_stats stats;
	struct sk_buff *tx_skb;		/* Packet being eaten by bus master ctrl.  */

	/* PCI configuration space information. */
	u8 pci_bus, pci_devfn;		/* PCI bus location, for power management. */
	char *cb_fn_base;			/* CardBus function status addr space. */
	int chip_id;

	/* The remainder are related to chip state, mostly media selection. */
	unsigned long in_interrupt;
	KTIMER timer;	/* Media selection timer. */
	int options;				/* User-settable misc. driver options. */
	unsigned int media_override:3, 			/* Passed-in media type. */
		default_media:4,				/* Read from the EEPROM/Wn3_Config. */
		full_duplex:1, force_fd:1, autoselect:1,
		bus_master:1,				/* Vortex can only do a fragment bus-m. */
		full_bus_master_tx:1, full_bus_master_rx:2, /* Boomerang  */
		hw_csums:1,				/* Has hardware checksums. */
		tx_full:1;
	u16 status_enable;
	u16 intr_enable;
	u16 available_media;				/* From Wn3_Options. */
	u16 capabilities, info1, info2;		/* Various, from EEPROM. */
	u16 advertising;					/* NWay media advertisement */
	unsigned char phys[2];				/* MII device addresses. */
};

/* The action to take with a media selection timer tick.
   Note that we deviate from the 3Com order by checking 10base2 before AUI.
 */
enum xcvr_types {
	XCVR_10baseT=0, XCVR_AUI, XCVR_10baseTOnly, XCVR_10base2, XCVR_100baseTx,
	XCVR_100baseFx, XCVR_MII=6, XCVR_NWAY=8, XCVR_ExtMII=9, XCVR_Default=10,
};

static struct media_table {
	char *name;
	unsigned int media_bits:16,		/* Bits to set in Wn4_Media register. */
		mask:8,				/* The transceiver-present bit in Wn3_Config.*/
		next:8;				/* The media type to try next. */
	int wait;			/* Time before we check media status. */
} media_tbl[] = {
  {	"10baseT",   Media_10TP,0x08, XCVR_10base2, (14*HZ)/10},
  { "10Mbs AUI", Media_SQE, 0x20, XCVR_Default, (1*HZ)/10},
  { "undefined", 0,			0x80, XCVR_10baseT, 10000},
  { "10base2",   0,			0x10, XCVR_AUI,		(1*HZ)/10},
  { "100baseTX", Media_Lnk, 0x02, XCVR_100baseFx, (14*HZ)/10},
  { "100baseFX", Media_Lnk, 0x04, XCVR_MII,		(14*HZ)/10},
  { "MII",		 0,			0x41, XCVR_10baseT, 3*HZ },
  { "undefined", 0,			0x01, XCVR_10baseT, 10000},
  { "Autonegotiate", 0,		0x41, XCVR_10baseT, 3*HZ},
  { "MII-External",	 0,		0x41, XCVR_10baseT, 3*HZ },
  { "Default",	 0,			0xFF, XCVR_10baseT, 10000},
};

static int vortex_scan(struct device *dev, struct pci_id_info pci_tbl[]);
static int vortex_open(struct device *dev);
static void mdio_sync(long ioaddr, int bits);
static int mdio_read(long ioaddr, int phy_id, int location);
static void mdio_write(long ioaddr, int phy_id, int location, int value);
static void vortex_timer(unsigned long arg);
static int vortex_start_xmit(struct sk_buff *skb, struct device *dev);
static int boomerang_start_xmit(struct sk_buff *skb, struct device *dev);
static int vortex_rx(struct device *dev);
static int boomerang_rx(struct device *dev);
//static void vortex_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void vortex_interrupt(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);
static int vortex_close(struct device *dev);
static void update_stats(long ioaddr, struct device *dev);
static struct net_device_stats *vortex_get_stats(struct device *dev);
static int vortex_get_status(struct device *dev);
static void set_rx_mode(struct device *dev);
static int vortex_ioctl(struct device *dev, struct ifreq *rq, int cmd);

KDPC TimerDpc_3c59x;
KTIMER Timer_3c59x;

KDPC InterruptTimerDpc_3c59x;
KTIMER InterruptTimer_3c59x;

/* This driver uses 'options' to pass the media type, full-duplex flag, etc. */
/* Option count limit only -- unlimited interfaces are supported. */
#define MAX_UNITS 8
static int options[MAX_UNITS] = { -1, -1, -1, -1, -1, -1, -1, -1,};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
/* A list of all installed Vortex devices, for removing the driver module. */
static struct device *root_vortex_dev = NULL;

/* Variables to work-around the Compaq PCI BIOS32 problem. */
static int compaq_ioaddr = 0, compaq_irq = 0, compaq_device_id = 0x5900;

int tc59x_probe(struct device *dev)
{
	static int scanned=0;
	if(scanned++)
		return -ENODEV;

	NbDebugPrint(0, ("%s", version));

	return vortex_scan(dev, pci_tbl);
}

static int vortex_scan(struct device *dev, struct pci_id_info pci_tbl[])
{
	int cards_found = 0;

//	DEBUGCALLXY(1, 16, "votex_scan start ", 7);
	NbDebugPrint(1, ("votex_scan start\n"));
	
	/* Ideally we would detect all cards in slot order.  That would
	   be best done a central PCI probe dispatch, which wouldn't work
	   well with the current structure.  So instead we detect 3Com cards
	   in slot order. */
	if (pcibios_present()) {
//		static int pci_index = 0;
		static unsigned char pci_index = 0;
		unsigned char pci_bus, pci_device_fn;

		for (;pci_index < 0xff; pci_index++) {
			u16 vendor, device, pci_command, new_command, pwr_cmd;
			int chip_idx, irq;
			long ioaddr;

			if (pcibios_find_class (PCI_CLASS_NETWORK_ETHERNET << 8, pci_index,
									&pci_bus, &pci_device_fn)
				!= PCIBIOS_SUCCESSFUL)
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
#if LINUX_VERSION_CODE >= 0x20155
				struct pci_dev *pdev = pci_find_slot(pci_bus, pci_device_fn);
				ioaddr = pdev->base_address[0] & ~3;
				irq = pdev->irq;
#else
				u32 pci_ioaddr;
				u8 pci_irq_line;
				pcibios_read_config_byte(pci_bus, pci_device_fn,
										 PCI_INTERRUPT_LINE, &pci_irq_line);
				pcibios_read_config_dword(pci_bus, pci_device_fn,
										  PCI_BASE_ADDRESS_0, &pci_ioaddr);
				ioaddr = pci_ioaddr & ~3;;
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

			/* Power-up the card. */

/*
			pcibios_read_config_word(pci_bus, pci_device_fn,
										 0xe0, &pwr_cmd);
			if (pwr_cmd & 0x3) {
				
				NbDebugPrint(1, ("  A 3Com network adapter is powered down!"
					   "  Setting the power state %4.4x->%4.4x.\n",
					   pwr_cmd, pwr_cmd & ~3));

				pcibios_write_config_word(pci_bus, pci_device_fn,
										  (u8)0xe0, (u16)(pwr_cmd & ~3));
				
				NbDebugPrint(1, ("  Setting the IRQ to %d, IOADDR to %#lx.\n",
					   irq, ioaddr));

				pcibios_write_config_byte(pci_bus, pci_device_fn,
										 PCI_INTERRUPT_LINE, (u8)irq);
				pcibios_write_config_dword(pci_bus, pci_device_fn,
										  PCI_BASE_ADDRESS_0, (u32)ioaddr);
			}
*/
			dev = vortex_probe1(pci_bus, pci_device_fn, dev, ioaddr, irq,
								chip_idx, cards_found);

			if (dev) {
				/* Get and check the latency values.  On the 3c590 series
				   the latency timer must be set to the maximum value to avoid
				   data corruption that occurs when the timer expires during
				   a transfer -- a bug in the Vortex chip only. */
				u8 pci_latency;
				u8 new_latency = (device & 0xff00) == 0x5900 ? 248 : 32;
				
				pcibios_read_config_byte(pci_bus, pci_device_fn,
										 PCI_LATENCY_TIMER, &pci_latency);
				if (pci_latency < new_latency) {					
					NbDebugPrint(1, ("%s: Overriding PCI latency"
						   " timer (CFLT) setting of %d, new value is %d.\n",
						   dev->name, pci_latency, new_latency));
					pcibios_write_config_byte(pci_bus, pci_device_fn,
											  PCI_LATENCY_TIMER, new_latency);
				}

				dev = 0;
				cards_found++;
			}
		}
	}

	/* Special code to work-around the Compaq PCI BIOS32 problem. */
	if (compaq_ioaddr) {
		vortex_probe1(0, 0, dev, compaq_ioaddr, compaq_irq,
					  compaq_device_id, cards_found++);
		dev = 0;
	}

	NbDebugPrint(1, ("votex_scan end, cards_found = %d\n", cards_found));

	return cards_found ? 0 : -ENODEV;
}

static struct device *vortex_probe1(int pci_bus, int pci_devfn,
									struct device *dev, long ioaddr,
									int irq, int chip_idx, int card_idx)
{
	struct vortex_private *vp;
	int option;
	unsigned int eeprom[0x40], checksum = 0;		/* EEPROM contents */
	int i;
	LARGE_INTEGER interval;

//	dev = init_etherdev(dev, 0);

	NbDebugPrint(1, ("vortex_probe1 %s: 3Com %s at 0x%lx\n ",
		   dev->name, pci_tbl[chip_idx].name, ioaddr));

	dev->base_addr = ioaddr;
	dev->irq = irq;
	dev->mtu = mtu;

	/* Make certain the descriptor lists are aligned. */
	{
		void *mem = kmalloc(sizeof(*vp) + 15, GFP_KERNEL);
		vp =  (void *)(((long)mem + 15) & ~15);
		memset(vp, 0, sizeof(*vp));
		vp->priv_addr = mem;
	}	
	dev->priv = vp;

	vp->next_module = root_vortex_dev;
	root_vortex_dev = dev;

	vp->chip_id = chip_idx;
	vp->pci_bus = (unsigned char)pci_bus;
	vp->pci_devfn = (unsigned char)pci_devfn;

	NbDebugPrint(1, ("vortex_probe1 :dev->mem_start = %lx\n ",dev->mem_start));

	/* The lower four bits are the media type. */
	if (dev->mem_start)
		option = dev->mem_start;
	else if (card_idx < MAX_UNITS)
		option = options[card_idx];
	else
		option = -1;

	NbDebugPrint(1, ("vortex_probe1 :option = %lx\n ", option));
	
	if (option >= 0) {
		vp->media_override = ((option & 7) == 2)  ?  0  :  option & 7;
		vp->full_duplex = (option & 8) ? 1 : 0;
		vp->bus_master = (option & 16) ? 1 : 0;
	} else {
		vp->media_override = 7;
		vp->full_duplex = 0;
		vp->bus_master = 0;
	}
	if (card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		vp->full_duplex = 1;

	vp->force_fd = vp->full_duplex;
	vp->options = option;

	NbDebugPrint(1, ( "vortex_probe1 :vp->media_override = %lx, vp->full_duplex = %lx,  vp->bus_master = %lx\n ", 
			vp->media_override, 
			vp->full_duplex, 
			vp->bus_master
		));
	
	/* Read the station address from the EEPROM. */
	EL3WINDOW(0);
	for (i = 0; i < 0x40; i++) {
		int timer;

		outw(EEPROM_Read + i, ioaddr + Wn0EepromCmd);

		/* Pause for at least 162 us. for the read to take place. */
		for (timer = 10; timer >= 0; timer--) {
			interval.QuadPart = - 1;
//			KeDelayExecutionThread(KernelMode, FALSE, &interval);
			KeDelayExecution(50);

			if ((inw(ioaddr + Wn0EepromCmd) & 0x8000) == 0)
				break;
		}
		eeprom[i] = inw(ioaddr + Wn0EepromData);
	}

	NbDebugPrint(1, ("EEPROM Data\n"));
	for (i = 0; i < 0x40; i++) {
		NbDebugPrint(1, (":%04x", eeprom[i]));
		if((i+1) % 0x08 == 0) NbDebugPrint(1,( "\n"));
	}
	NbDebugPrint(1, ("\n"));

	for(i=0;i<0x18;i++)
		checksum ^= eeprom[i];

	checksum = (checksum ^ (checksum >> 8)) & 0xff;
	if (checksum != 0x00) {		/* Grrr, needless incompatible change 3Com. */
		while (i < 0x21)
			checksum ^= eeprom[i++];
		checksum = (checksum ^ (checksum >> 8)) & 0xff;
	}
	if (checksum != 0x00) {	
		NbDebugPrint(1, (" ***INVALID CHECKSUM %4.4x*** ", checksum));
	}

	for (i = 0; i < 3; i++)
		((u16 *)dev->dev_addr)[i] = HTONS(eeprom[i + 10]);
	for (i = 0; i < 6; i++) {		
		NbDebugPrint(1, ("%c%2.2x", i ? ':' : ' ', dev->dev_addr[i]));
	}
	
	NbDebugPrint(1, (", IRQ %d\n", dev->irq));

	/* Extract our information from the EEPROM data. */
	vp->info1 = (unsigned short)eeprom[13];
	vp->info2 = (unsigned short)eeprom[15];
	vp->capabilities = (unsigned short)eeprom[16];

	if (vp->info1 & 0x8000)
		vp->full_duplex = 1;

	{
		char *ram_split[] = {"5:3", "3:1", "1:1", "3:5"};
		union wn3_config config;
		EL3WINDOW(3);
		vp->available_media = inw(ioaddr + Wn3_Options);
		if ((vp->available_media & 0xff) == 0)		/* Broken 3c916 */
			vp->available_media = 0x40;
		config.i = inl(ioaddr + Wn3_Config);
		if (vortex_debug > 1) {			
			NbDebugPrint(1, ( "  Internal config register is %4.4x, "
				   "transceivers %#x.\n", config.i, inw(ioaddr + Wn3_Options)));

		}	

		NbDebugPrint(1, ( "  %dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
			   8 << config.u.ram_size,
			   config.u.ram_width ? "word" : "byte",
			   ram_split[config.u.ram_split],
			   config.u.autoselect ? "autoselect/" : "",
			   config.u.xcvr > XCVR_ExtMII ? "<invalid transceiver>" :
			   media_tbl[config.u.xcvr].name));
		vp->default_media = config.u.xcvr;
		vp->autoselect = config.u.autoselect;
	}

	if (vp->media_override != 7) {		
		NbDebugPrint(1, ( "  Media override to transceiver type %d (%s).\n",
			   vp->media_override, media_tbl[vp->media_override].name));
		dev->if_port = (unsigned char)vp->media_override;
	} else
		dev->if_port = (unsigned char)vp->default_media;

	if (dev->if_port == XCVR_MII || dev->if_port == XCVR_NWAY) {
	/*	int phy, phy_idx = 0;
		EL3WINDOW(4);
		mii_preamble_required++;
		mii_preamble_required++;
		mdio_read(ioaddr, 24, 1);
		for (phy = 1; phy <= 32 && phy_idx < sizeof(vp->phys); phy++) {
			int mii_status, phyx = phy & 0x1f;
			mii_status = mdio_read(ioaddr, phyx, 1);
			if (mii_status  &&  mii_status != 0xffff) {
				vp->phys[phy_idx++] = (unsigned char)phyx;				
				NbDebugPrint(1, ( "  MII transceiver found at address %d,"
					   " status %4x.\n", phyx, mii_status));
				if ((mii_status & 0x0040) == 0)
					mii_preamble_required++;
			}
		}
		mii_preamble_required--;
		if (phy_idx == 0) {			
			NbDebugPrint(1, ("  ***WARNING*** No MII transceivers found!\n"));
			vp->phys[0] = 24;
		} else {
			vp->advertising = (unsigned short)mdio_read(ioaddr, vp->phys[0], 4);
			if (vp->full_duplex) {
				/* Only advertise the FD media types. */
	/*			vp->advertising &= ~0x02A0;
				mdio_write(ioaddr, vp->phys[0], 4, vp->advertising);
			}
		}
	*/
		int phy, phy_idx = 0;
		EL3WINDOW(4);
		mii_preamble_required++;
		mii_preamble_required++;
		mdio_read(ioaddr, 24, 1);
		for (phy = 0; phy < 32 && phy_idx < 1; phy++) {
			int mii_status, phyx;

			/*
			 * For the 3c905CX we look at index 24 first, because it bogusly
			 * reports an external PHY at all indices
			 */
			if (phy == 0)
				phyx = 24;
			else if (phy <= 24)
				phyx = phy - 1;
			else
				phyx = phy;
			mii_status = mdio_read(ioaddr, phyx, 1);
			if (mii_status  &&  mii_status != 0xffff) {
				vp->phys[phy_idx++] = (unsigned char)phyx;
				
				NbDebugPrint(1, ( "  MII transceiver found at address %d,"
				   " status %4x.\n", phyx, mii_status));	

				if ((mii_status & 0x0040) == 0)
					mii_preamble_required++;
			}
		}
		mii_preamble_required--;
		if (phy_idx == 0) {			
			NbDebugPrint(1, ("  ***WARNING*** No MII transceivers found!\n"));
			vp->phys[0] = 24;
		} else {
			vp->advertising = (unsigned short)mdio_read(ioaddr, vp->phys[0], 4);
			if (vp->full_duplex) {
				/* Only advertise the FD media types. */
				vp->advertising &= ~0x02A0;
				mdio_write(ioaddr, vp->phys[0], 4, vp->advertising);
			}
		}
	}

	if (vp->capabilities & CapBusMaster) {
		vp->full_bus_master_tx = 1;		
		NbDebugPrint(1, ( "  Enabling bus-master transmits and %s receives.\n",
			   (vp->info2 & 1) ? "early" : "whole-frame" ));
		vp->full_bus_master_rx = (vp->info2 & 1) ? 1 : 2;
		vp->bus_master  = 0;
	}

	/* We do a request_region() to register /proc/ioports info. */
//	request_region(ioaddr, pci_tbl[chip_idx].io_size, dev->name);

	/* The 3c59x-specific entries in the device structure. */
	dev->open = &vortex_open;
	dev->hard_start_xmit = &vortex_start_xmit;
	dev->stop = &vortex_close;
	dev->get_status = vortex_get_status;
//	dev->get_stats = &vortex_get_stats;
//	dev->do_ioctl = &vortex_ioctl;
//	dev->set_multicast_list = &set_rx_mode;

	{
		NbDebugPrint(1, ( "3C59X ETHER_ADDR"));
		for(i=0;i<6;i++) {
			NbDebugPrint(1, ( ":%02x", NetDevice.dev_addr[i]));
		}
		NbDebugPrint(1, ("\n"));

	}
	return dev;
}


static int
vortex_open(struct device *dev)
{
	long ioaddr = dev->base_addr;
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	union wn3_config config;
	int i;
	LARGE_INTEGER deltaTime_3c59x;

	NbDebugPrint(1, ("votex_open start, irq = %d\n", dev->irq));

#ifndef __INTERRUPT__
	HalDisableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		PASSIVE_LEVEL
	);
#endif

	/////////////////////////////////////////////////////////////////////////
//	nic_irq = dev->irq;
	/////////////////////////////////////////////////////////////////////////

	/* Before initializing select the active media port. */
	EL3WINDOW(3);
	config.i = inl(ioaddr + Wn3_Config);

	if (vp->media_override != 7) {
		if (vortex_debug > 1) {			
			NbDebugPrint(1, ("%s: Media override to transceiver %d (%s).\n",
				   dev->name, vp->media_override,
				   media_tbl[vp->media_override].name));
		}
		dev->if_port = (unsigned char)vp->media_override;
	} else if (vp->autoselect && pci_tbl[vp->chip_id].drv_flags & HAS_NWAY) {
			dev->if_port = XCVR_NWAY;
	} else if (vp->autoselect) {
		/* Find first available media type, starting with 100baseTx. */
		dev->if_port = XCVR_100baseTx;
		while (! (vp->available_media & media_tbl[dev->if_port].mask))
			dev->if_port = (unsigned char)media_tbl[dev->if_port].next;
	} else
		dev->if_port = (unsigned char)vp->default_media;

	if (vortex_debug > 1) {		
		NbDebugPrint(1, (  "%s: Initial media type %s.\n",
			   dev->name, media_tbl[dev->if_port].name));
	}

	vp->full_duplex = vp->force_fd;
	config.u.xcvr = dev->if_port;
	outl(config.i, ioaddr + Wn3_Config);

	if (dev->if_port == XCVR_MII || dev->if_port == XCVR_NWAY) {
		int mii_reg1, mii_reg5;
		EL3WINDOW(4);
		/* Read BMSR (reg1) only to clear old status. */
		mii_reg1 = mdio_read(ioaddr, vp->phys[0], 1);
		mii_reg5 = mdio_read(ioaddr, vp->phys[0], 5);
		if (mii_reg5 == 0xffff  ||  mii_reg5 == 0x0000)
			;					/* No MII device or no link partner report */
		else if ((mii_reg5 & 0x0100) != 0	/* 100baseTx-FD */
				 || (mii_reg5 & 0x00C0) == 0x0040) /* 10T-FD, but not 100-HD */
			vp->full_duplex = 1;
		if (vortex_debug > 1) {			
			NbDebugPrint(1, ( "%s: MII #%d status %4.4x, link partner capability %4.4x,"
				   " setting %s-duplex.\n", dev->name, vp->phys[0],
				   mii_reg1, mii_reg5, vp->full_duplex ? "full" : "half"));
		}
		EL3WINDOW(3);
	}

	/* Set the full-duplex bit. */
	outb(((vp->info1 & 0x8000) || vp->full_duplex ? 0x20 : 0) |
		 (dev->mtu > 1500 ? 0x40 : 0), ioaddr + Wn3_MAC_Ctrl);

	if (vortex_debug > 1) {		
		NbDebugPrint(1, ( "%s: vortex_open() InternalConfig %8.8x.\n",
			dev->name, config.i));
	}

	outw(TxReset, ioaddr + EL3_CMD);
	for (i = 2000; i >= 0 ; i--)
		if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
			break;

	outw(RxReset, ioaddr + EL3_CMD);
	/* Wait a few ticks for the RxReset command to complete. */
	for (i = 2000; i >= 0 ; i--)
		if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
			break;

	outw(SetStatusEnb | 0x00, ioaddr + EL3_CMD);

	/* Use the now-standard shared IRQ implementation. */
//	if (request_irq(dev->irq, &vortex_interrupt, SA_SHIRQ, dev->name, dev)) {
//		return -EAGAIN;
//	}

	if (vortex_debug > 1) {
		EL3WINDOW(4);		
		NbDebugPrint(1, ( "%s: vortex_open() irq %d media status %4.4x.\n",
			   dev->name, dev->irq, inw(ioaddr + Wn4_Media)));
	}

	/* Set the station address and mask in window 2 each time opened. */
	EL3WINDOW(2);
	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + i);
	for (; i < 12; i+=2)
		outw(0, ioaddr + i);

	if (dev->if_port == XCVR_10base2)
		/* Start the thinnet transceiver. We should really wait 50ms...*/
		outw(StartCoax, ioaddr + EL3_CMD);
	if (dev->if_port != XCVR_NWAY) {
		EL3WINDOW(4);
		outw((inw(ioaddr + Wn4_Media) & ~(Media_10TP|Media_SQE)) |
			 media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);
	}

	/* Switch to the stats window, and clear all stats by reading. */
	outw(StatsDisable, ioaddr + EL3_CMD);
	EL3WINDOW(6);
	for (i = 0; i < 10; i++)
		inb(ioaddr + i);
	inw(ioaddr + 10);
	inw(ioaddr + 12);
	/* New: On the Vortex we must also clear the BadSSD counter. */
	EL3WINDOW(4);
	inb(ioaddr + 12);
	/* ..and on the Boomerang we enable the extra statistics bits. */
	outw(0x0040, ioaddr + Wn4_NetDiag);

	/* Switch to register set 7 for normal use. */
	EL3WINDOW(7);

	if (vp->full_bus_master_rx) { /* Boomerang bus master. */
		vp->cur_rx = vp->dirty_rx = 0;
		/* Initialize the RxEarly register as recommended. */
		outw(SetRxThreshold + (1536>>2), ioaddr + EL3_CMD);
		outl(0x0020, ioaddr + PktStatus);
		if (vortex_debug > 2)
			NbDebugPrint(0, ( "%s:  Filling in the Rx ring.\n", dev->name));

		for (i = 0; i < RX_RING_SIZE; i++) {
			struct sk_buff *skb;
			vp->rx_ring[i].next = cpu_to_le32(virt_to_bus(&vp->rx_ring[i+1]));
			vp->rx_ring[i].status = 0;	/* Clear complete bit. */
			vp->rx_ring[i].length = cpu_to_le32(PKT_BUF_SZ | LAST_FRAG);
			skb = dev_alloc_skb(PKT_BUF_SZ);
			vp->rx_skbuff[i] = skb;
			if (skb == NULL)
				break;			/* Bad news!  */
			skb->dev = dev;			/* Mark as being used by this device. */
#if LINUX_VERSION_CODE >= 0x10300
			skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
			vp->rx_ring[i].addr = cpu_to_le32(virt_to_bus(skb->tail));
#else
			vp->rx_ring[i].addr = virt_to_bus(skb->data);
#endif
		}
		/* Wrap the ring. */
		vp->rx_ring[i-1].next = cpu_to_le32(virt_to_bus(&vp->rx_ring[0]));
		outl(virt_to_bus(&vp->rx_ring[0]), ioaddr + UpListPtr);
	}
	if (vp->full_bus_master_tx) { 		/* Boomerang bus master Tx. */
		dev->hard_start_xmit = &boomerang_start_xmit;
		vp->cur_tx = vp->dirty_tx = 0;
		outb(PKT_BUF_SZ>>8, ioaddr + TxFreeThreshold); /* Room for a packet. */
		/* Clear the Tx ring. */
		for (i = 0; i < TX_RING_SIZE; i++)
			vp->tx_skbuff[i] = 0;
		outl(0, ioaddr + DownListPtr);
	}
	/* Set reciever mode: presumably accept b-case and phys addr only. */
	set_rx_mode(dev);
	outw(StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

	vp->in_interrupt = 0;
	dev->tbusy = 0;
	dev->interrupt = 0;
	dev->start = 1;

	outw(RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
	outw(TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */
	/* Allow status bits to be seen. */
	vp->status_enable = SetStatusEnb | HostError|IntReq|StatsFull|TxComplete|
		(vp->full_bus_master_tx ? DownComplete : TxAvailable) |
		(vp->full_bus_master_rx ? UpComplete : RxComplete) |
		(vp->bus_master ? DMADone : 0);
	vp->intr_enable = SetIntrEnb | IntLatch | TxAvailable | RxComplete |
		StatsFull | HostError | TxComplete | IntReq
		| (vp->bus_master ? DMADone : 0) | UpComplete | DownComplete;
	outw(vp->status_enable, ioaddr + EL3_CMD);
	/* Ack all pending events, and set active indicator mask. */
	outw(AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
		 ioaddr + EL3_CMD);
	outw(vp->intr_enable, ioaddr + EL3_CMD);

//	KeInitializeDpc( &TimerDpc_3c59x, vortex_timer, dev );
//	KeInitializeTimer( &Timer_3c59x );
//	deltaTime.QuadPart = - (24 * HZ) / 10;    /* 2.4 sec. */
//  (VOID) KeSetTimer( &Timer_3c59x, deltaTime_3c59x, &TimerDpc_3c59x );

	KeInitializeDpc( &InterruptTimerDpc_3c59x, vortex_interrupt, dev );
	KeInitializeTimer( &InterruptTimer_3c59x );

	deltaTime_3c59x.QuadPart = - INTERRUPT_TIME;    /* 2.4 sec. */
	KeSetTimer( &InterruptTimer_3c59x, deltaTime_3c59x, &InterruptTimerDpc_3c59x );

	NbDebugPrint(1, ( "votex_open end\n"));	

	return 0;
}

static void vortex_timer(unsigned long data)
{
	struct device *dev = (struct device *)data;
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 0;
	int ok = 0;
	int media_status, mii_status, old_window;
	LARGE_INTEGER deltaTime_3c59x;

	///////////////////////////////////////////
	KeCancelTimer(&Timer_3c59x);
	///////////////////////////////////////////

	if (vortex_debug > 1)
		NbDebugPrint(0, ("%s: Media selection timer tick happened, %s.\n",
			   dev->name, media_tbl[dev->if_port].name));
	
	old_window = inw(ioaddr + EL3_CMD) >> 13;
	EL3WINDOW(4);
	media_status = inw(ioaddr + Wn4_Media);
	switch (dev->if_port) {
	case XCVR_10baseT:  case XCVR_100baseTx:  case XCVR_100baseFx:
		if (media_status & Media_LnkBeat) {
		  ok = 1;
		  if (vortex_debug > 1)
			NbDebugPrint(0, ( "%s: Media %s has link beat, %x.\n",
				   dev->name, media_tbl[dev->if_port].name, media_status));
		} else if (vortex_debug > 1)
		  NbDebugPrint(0, ( "%s: Media %s is has no link beat, %x.\n",
				   dev->name, media_tbl[dev->if_port].name, media_status));
		break;
	  case XCVR_MII: case XCVR_NWAY:
		  mii_status = mdio_read(ioaddr, vp->phys[0], 1);
		  ok = 1;
		  if (debug > 1)
			  NbDebugPrint(0, ( "%s: MII transceiver has status %4.4x.\n",
					 dev->name, mii_status));
		  if (mii_status & 0x0004) {
			  int mii_reg5 = mdio_read(ioaddr, vp->phys[0], 5);
			  if (! vp->force_fd  &&  mii_reg5 != 0xffff) {
				  unsigned int duplex = (mii_reg5&0x0100) ||
					  (mii_reg5 & 0x01C0) == 0x0040;
				  if (vp->full_duplex != duplex) {
					  vp->full_duplex = duplex;
					  NbDebugPrint(0, ("%s: Setting %s-duplex based on MII "
							 "#%d link partner capability of %4.4x.\n",
							 dev->name, vp->full_duplex ? "full" : "half",
							 vp->phys[0], mii_reg5));
					  /* Set the full-duplex bit. */
					  outb((vp->full_duplex ? 0x20 : 0) |
						   (dev->mtu > 1500 ? 0x40 : 0),
						   ioaddr + Wn3_MAC_Ctrl);
				  }
				  next_tick = 60*HZ;
			  }
		  }
		  break;
	  default:					/* Other media types handled by Tx timeouts. */
		if (vortex_debug > 1)
		  NbDebugPrint(0, ("%s: Media %s is has no indication, %x.\n",
				 dev->name, media_tbl[dev->if_port].name, media_status));
		ok = 1;
	}
	if ( ! ok) {
		union wn3_config config;

		do {
			dev->if_port = (unsigned char)media_tbl[dev->if_port].next;
		} while ( ! (vp->available_media & media_tbl[dev->if_port].mask));
		if (dev->if_port == XCVR_Default) { /* Go back to default. */
		  dev->if_port = (unsigned char)vp->default_media;
		  if (vortex_debug > 1)
			NbDebugPrint(0, ( "%s: Media selection failing, using default "
				   "%s port.\n",
				   dev->name, media_tbl[dev->if_port].name));
		} else {
		  if (vortex_debug > 1)
			NbDebugPrint(0, ( "%s: Media selection failed, now trying "
				   "%s port.\n",
				   dev->name, media_tbl[dev->if_port].name));
		  next_tick = media_tbl[dev->if_port].wait;
		}
		outw((media_status & ~(Media_10TP|Media_SQE)) |
			 media_tbl[dev->if_port].media_bits, ioaddr + Wn4_Media);

		EL3WINDOW(3);
		config.i = inl(ioaddr + Wn3_Config);
		config.u.xcvr = dev->if_port;
		outl(config.i, ioaddr + Wn3_Config);

		outw(dev->if_port == XCVR_10base2 ? StartCoax : StopCoax,
			 ioaddr + EL3_CMD);
	}
	EL3WINDOW(old_window);	

	if (vortex_debug > 2)
	  NbDebugPrint(0, ( "%s: Media selection timer finished, %s.\n",
			 dev->name, media_tbl[dev->if_port].name));

	if(next_tick) {
		deltaTime_3c59x.QuadPart = - next_tick;
		KeSetTimer( &Timer_3c59x, deltaTime_3c59x, &TimerDpc_3c59x );
	}

	return;
}

static void vortex_tx_timeout(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int j;

	NbDebugPrint(0, ( "%s: transmit timed out, tx_status %2.2x status %4.4x.\n",
		   dev->name, inb(ioaddr + TxStatus),
		   inw(ioaddr + EL3_STATUS)));
	/* Slight code bloat to be user friendly. */
	if ((inb(ioaddr + TxStatus) & 0x88) == 0x88)
		NbDebugPrint(0, ( "%s: Transmitter encountered 16 collisions --"
			   " network cable problem?\n", dev->name));
	if (inw(ioaddr + EL3_STATUS) & IntLatch) {
		NbDebugPrint(0, ( "%s: Interrupt posted but not delivered --"
			   " IRQ blocked by another device?\n", dev->name));
		/* Bad idea here.. but we might as well handle a few events. */
//		vortex_interrupt(dev->irq, dev, 0);
		vortex_interrupt(NULL, dev, 0, 0);
	}
	outw(TxReset, ioaddr + EL3_CMD);
	for (j = 200; j >= 0 ; j--)
		if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
			break;

#if ! defined(final_version) && LINUX_VERSION_CODE >= 0x10300
	if (vp->full_bus_master_tx) {
		int i;
		NbDebugPrint(0, ( "  Flags; bus-master %d, full %d; dirty %d "
			   "current %d.\n",
			   vp->full_bus_master_tx, vp->tx_full, vp->dirty_tx, vp->cur_tx));
		NbDebugPrint(0, ( "  Transmit list %8.8x vs. %p.\n",
			   inl(ioaddr + DownListPtr),
			   &vp->tx_ring[vp->dirty_tx % TX_RING_SIZE]));
		for (i = 0; i < TX_RING_SIZE; i++) {
			NbDebugPrint(0, ( "  %d: @%p  length %8.8x status %8.8x\n", i,
				   &vp->tx_ring[i],
				   le32_to_cpu(vp->tx_ring[i].length),
				   le32_to_cpu(vp->tx_ring[i].status)));
		}
	}
#endif

	vp->stats.tx_errors++;
	if (vp->full_bus_master_tx) {
		if (vortex_debug > 0)
			NbDebugPrint(0, ("%s: Resetting the Tx ring pointer.\n",
				   dev->name));
		if (vp->cur_tx - vp->dirty_tx > 0  &&  inl(ioaddr + DownListPtr) == 0)
			outl(virt_to_bus(&vp->tx_ring[vp->dirty_tx % TX_RING_SIZE]),
				 ioaddr + DownListPtr);
		if (vp->tx_full && (vp->cur_tx - vp->dirty_tx <= TX_RING_SIZE - 1)) {
			vp->tx_full = 0;
			clear_bit(0, (void*)&dev->tbusy);
		}
		outb(PKT_BUF_SZ>>8, ioaddr + TxFreeThreshold);
		outw(DownUnstall, ioaddr + EL3_CMD);
	} else
		vp->stats.tx_dropped++;
	
	/* Issue Tx Enable */
	outw(TxEnable, ioaddr + EL3_CMD);
	dev->trans_start = jiffies;
	
	/* Switch to register set 7 for normal use. */
	EL3WINDOW(7);
}

/*
 * Handle uncommon interrupt sources.  This is a separate routine to minimize
 * the cache impact.
 */
static void
vortex_error(struct device *dev, int status)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int do_tx_reset = 0;
	int i;

	if (status & TxComplete) {			/* Really "TxError" for us. */
		unsigned char tx_status = inb(ioaddr + TxStatus);
		/* Presumably a tx-timeout. We must merely re-enable. */
		if (vortex_debug > 2
			|| (tx_status != 0x88 && vortex_debug > 0))
			NbDebugPrint(0, ("%s: Transmit error, Tx status register %2.2x.\n",
				   dev->name, tx_status));
		if (tx_status & 0x14)  vp->stats.tx_fifo_errors++;
		if (tx_status & 0x38)  vp->stats.tx_aborted_errors++;
		outb(0, ioaddr + TxStatus);
		if (tx_status & 0x30)
			do_tx_reset = 1;
		else					/* Merely re-enable the transmitter. */
			outw(TxEnable, ioaddr + EL3_CMD);
	}
	if (status & RxEarly) {				/* Rx early is unused. */
		vortex_rx(dev);
		outw(AckIntr | RxEarly, ioaddr + EL3_CMD);
	}
	if (status & StatsFull) {			/* Empty statistics. */
		static int DoneDidThat = 0;
		if (vortex_debug > 4)
			NbDebugPrint(0, ( "%s: Updating stats.\n", dev->name));
		update_stats(ioaddr, dev);
		/* HACK: Disable statistics as an interrupt source. */
		/* This occurs when we have the wrong media type! */
		if (DoneDidThat == 0  &&
			inw(ioaddr + EL3_STATUS) & StatsFull) {
			NbDebugPrint(0, ( "%s: Updating statistics failed, disabling "
				   "stats as an interrupt source.\n", dev->name));
			EL3WINDOW(5);
			outw(SetIntrEnb | (inw(ioaddr + 10) & ~StatsFull), ioaddr + EL3_CMD);
			EL3WINDOW(7);
			DoneDidThat++;
		}
	}
	if (status & IntReq) {		/* Restore all interrupt sources.  */
		outw(vp->status_enable, ioaddr + EL3_CMD);
		outw(vp->intr_enable, ioaddr + EL3_CMD);
	}
	if (status & HostError) {
		u16 fifo_diag;
		EL3WINDOW(4);
		fifo_diag = inw(ioaddr + Wn4_FIFODiag);
		if (vortex_debug > 0)
			NbDebugPrint(0, ( "%s: Host error, FIFO diagnostic register %4.4x.\n",
				   dev->name, fifo_diag));
		/* Adapter failure requires Tx/Rx reset and reinit. */
		if (vp->full_bus_master_tx) {
			outw(TotalReset | 0xff, ioaddr + EL3_CMD);
			for (i = 2000; i >= 0 ; i--)
				if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
					break;
			/* Re-enable the receiver. */
			outw(RxEnable, ioaddr + EL3_CMD);
			outw(TxEnable, ioaddr + EL3_CMD);
		} else if (fifo_diag & 0x0400)
			do_tx_reset = 1;
		if (fifo_diag & 0x3000) {
			outw(RxReset, ioaddr + EL3_CMD);
			for (i = 2000; i >= 0 ; i--)
				if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
					break;
			/* Set the Rx filter to the current state. */
			set_rx_mode(dev);
			outw(RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver. */
			outw(AckIntr | HostError, ioaddr + EL3_CMD);
		}
	}
	if (do_tx_reset) {
		int j;
		outw(TxReset, ioaddr + EL3_CMD);
		for (j = 200; j >= 0 ; j--)
			if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
				break;
		outw(TxEnable, ioaddr + EL3_CMD);
	}

}


static int
vortex_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;

	if (test_and_set_bit(0, (void*)&dev->tbusy) != 0) {
		if (jiffies - dev->trans_start >= TX_TIMEOUT)
			vortex_tx_timeout(dev);
		return 1;
	}

	/* Put out the doubleword header... */
	outl(skb->len, ioaddr + TX_FIFO);
	if (vp->bus_master) {
		/* Set the bus-master controller to transfer the packet. */
		outl(virt_to_bus(skb->data), ioaddr + Wn7_MasterAddr);
		outw((skb->len + 3) & ~3, ioaddr + Wn7_MasterLen);
		vp->tx_skb = skb;
		outw(StartDMADown, ioaddr + EL3_CMD);
		/* dev->tbusy will be cleared at the DMADone interrupt. */
	} else {
		/* ... and the packet rounded to a doubleword. */
		outsl((unsigned short)(ioaddr + TX_FIFO), (void*)(skb->data), (skb->len + 3) >> 2);
		DEV_FREE_SKB(skb);
		if (inw(ioaddr + TxFree) > 1536) {
			clear_bit(0, (void*)&dev->tbusy);
		} else
			/* Interrupt us when the FIFO has room for max-sized packet. */
			outw(SetTxThreshold + (1536>>2), ioaddr + EL3_CMD);
	}

	dev->trans_start = jiffies;

	/* Clear the Tx status stack. */
	{
		int tx_status;
		int i = 32;

		while (--i > 0	&&	(tx_status = inb(ioaddr + TxStatus)) > 0) {
			if (tx_status & 0x3C) {		/* A Tx-disabling error occurred.  */
				if (vortex_debug > 2)
				  NbDebugPrint(0, ( "%s: Tx error, status %2.2x.\n",
						 dev->name, tx_status));
				if (tx_status & 0x04) vp->stats.tx_fifo_errors++;
				if (tx_status & 0x38) vp->stats.tx_aborted_errors++;
				if (tx_status & 0x30) {
					int j;
					outw(TxReset, ioaddr + EL3_CMD);
					for (j = 200; j >= 0 ; j--)
						if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
							break;
				}
				outw(TxEnable, ioaddr + EL3_CMD);
			}
			outb(0x00, ioaddr + TxStatus); /* Pop the status stack. */
		}
	}
	vp->stats.tx_bytes += skb->len;
	return 0;
}

static int
boomerang_start_xmit(struct sk_buff *skb, struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;

	NbDebugPrint(1, ("boomerang_start_xmit: dev->tbusy = %d\n", dev->tbusy));	
	outw(SetTxThreshold + (1536>>2), ioaddr + EL3_CMD);
	
	if (test_and_set_bit(0, (void*)&dev->tbusy) != 0) {
		NbDebugPrint(1, ("vortex_tx_timeout: dev->tbusy = %d\n", dev->tbusy));
		if (jiffies - dev->trans_start >= TX_TIMEOUT)
			vortex_tx_timeout(dev);
		return 1;
	} else {
		/* Calculate the next Tx descriptor entry. */
		int entry = vp->cur_tx % TX_RING_SIZE;
		struct boom_tx_desc *prev_entry =
			&vp->tx_ring[(vp->cur_tx-1) % TX_RING_SIZE];
		unsigned long flags = 0;
		int i;

		NbDebugPrint(1, ("vp->cur_tx: dev->tbusy = %d\n", dev->tbusy));
		
		if (vortex_debug > 3) {			
			NbDebugPrint(1, ("%s: Trying to send a packet, Tx index %d.\n",
				   dev->name, vp->cur_tx));
		}
		
		if (vp->tx_full) {
			if (vortex_debug >0) {				
				NbDebugPrint(1, ("%s: Tx Ring full, refusing to send buffer.\n",
					   dev->name));
			}
			return 1;
		}
		vp->tx_skbuff[entry] = skb;
		vp->tx_ring[entry].next = 0;
		vp->tx_ring[entry].addr = cpu_to_le32(virt_to_bus(skb->data));
		vp->tx_ring[entry].length = cpu_to_le32(skb->len | LAST_FRAG);
		vp->tx_ring[entry].status = cpu_to_le32(skb->len | TxIntrUploaded);

	/*
		{
			int length;
			int physicalCcb;
			
			physicalCcb = ScsiPortConvertPhysicalAddressToUlong(
	        ScsiPortGetPhysicalAddress(HwDeviceExt, NULL, &vp->tx_ring[entry], &length));

			NbDebugPrint(1, ("physical address of &vp->tx_ring[entry] = %lx\n", physicalCcb));
		}
	*/

		NbDebugPrint(1, ("entry = %lx\n", entry));		
		NbDebugPrint(1, ("vp->tx_ring[entry].addr = %lx\n", vp->tx_ring[entry].addr));
		NbDebugPrint(1, ("vp->tx_ring[entry].length = %lx\n", vp->tx_ring[entry].length));
		NbDebugPrint(1, ("vp->tx_ring[entry].status = %lx\n", vp->tx_ring[entry].status));

		NbDebugPrint(1, ("virt_to_bus(&vp->tx_ring[entry] = %lx\n", virt_to_bus(&vp->tx_ring[entry])));
		
		for(i=0;i<(int)skb->len;i++) {
			NbDebugPrint(1, ("%02x:", skb->data[i]));
			if((i+1) % 0x10 == 0) NbDebugPrint(1, ("\n"));
		}
		NbDebugPrint(1, ("\n"));
		
//		save_flags(flags);
//		cli();

		outw(DownStall, ioaddr + EL3_CMD);
		/* Wait for the stall to complete. */
		for (i = 600; i >= 0 ; i--) {
//			NbDebugPrint(1, ("Status = %lx.\n",  inw(ioaddr + EL3_STATUS)));
			
			if ( (inw(ioaddr + EL3_STATUS) & CmdInProgress) == 0)
				break;
		}
		prev_entry->next = cpu_to_le32(virt_to_bus(&vp->tx_ring[entry]));
		if (inl(ioaddr + DownListPtr) == 0) {
			NbDebugPrint(1, ("queued_packet = %d\n", queued_packet));			
			outl(virt_to_bus(&vp->tx_ring[entry]), ioaddr + DownListPtr);
			queued_packet++;
		}
		outw(DownUnstall, ioaddr + EL3_CMD);
//		restore_flags(flags);

//		NbDebugPrint(1, ("vp->cur_tx = %lx, vp->dirty_tx = %lx.\n",  vp->cur_tx, vp->dirty_tx));

		vp->cur_tx++;
		if (vp->cur_tx - vp->dirty_tx > TX_RING_SIZE - 1)
			vp->tx_full = 1;
		else {					/* Clear previous interrupt enable. */
			prev_entry->status &= cpu_to_le32(~TxIntrUploaded);
			clear_bit(0, (void*)&dev->tbusy);
		}
		dev->trans_start = jiffies;
		vp->stats.tx_bytes += skb->len;
//		NbDebugPrint(1, ("Trasmit time : dev->trans_start = %lx\n", dev->trans_start));
		return 0;
	}
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
//static void vortex_interrupt(int irq, void *dev_id, struct pt_regs *regs)
static void vortex_interrupt(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
	struct device *dev = (struct device *)DeferredContext;
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int latency, status;
	int work_done = max_interrupt_work;
	LARGE_INTEGER deltaTime_3c59x;

	KeCancelTimer(&InterruptTimer_3c59x);

	if( STATUS_NIC_OK != vortex_get_status(dev)) {
		NbDebugPrint (0, ("vortex_interrupt is corrupted !!!!!!!\n"));		
		return;
	}	

#if defined(__i386__)
	/* A lock to prevent simultaneous entry bug on Intel SMP machines. */
	if (test_and_set_bit(0, (void*)&dev->interrupt)) {		
		NbDebugPrint(1, ("%s: SMP simultaneous entry of an interrupt handler.\n",
			   dev->name));
		dev->interrupt = 0;	/* Avoid halting machine. */

		deltaTime_3c59x.QuadPart = - INTERRUPT_TIME;    
	    KeSetTimer( &InterruptTimer_3c59x, deltaTime_3c59x, &InterruptTimerDpc_3c59x );

		return;
	}
#else
	if (dev->interrupt) {	
		NbDebugPrint(1, ("%s: Re-entering the interrupt handler.\n", dev->name));

		deltaTime_3c59x.QuadPart = - INTERRUPT_TIME;    
	    KeSetTimer( &InterruptTimer_3c59x, deltaTime_3c59x, &InterruptTimerDpc_3c59x );

		return;
	}	
#endif

	dev->interrupt = 1;
	ioaddr = dev->base_addr;
	latency = inb(ioaddr + Timer);
	status = inw(ioaddr + EL3_STATUS);

	if (vortex_debug > 4) {		
		NbDebugPrint(1, ("%s: interrupt, status %4.4x, latency %d ticks.\n",
			   dev->name, status, latency));
	}
	
	do {
		if (vortex_debug > 5) {				
			NbDebugPrint(1, ("%s: In interrupt loop, status %4.4x.\n",
					   dev->name, status));
		}
		
		if (status & RxComplete)
			vortex_rx(dev);
		if (status & UpComplete) {
			outw(AckIntr | UpComplete, ioaddr + EL3_CMD);
			boomerang_rx(dev);
		}

		if (status & TxAvailable) {
			if (vortex_debug > 5) {				
				NbDebugPrint(1, ("	TX room bit was handled.\n"));
			}
			/* There's room in the FIFO for a full-sized packet. */
			outw(AckIntr | TxAvailable, ioaddr + EL3_CMD);
			clear_bit(0, (void*)&dev->tbusy);
//			mark_bh(NET_BH);
		}

		if (status & DownComplete) {
			unsigned int dirty_tx = vp->dirty_tx;

			NbDebugPrint(1, ("Tx status = %lx\n", inb(ioaddr + TxStatus)));
			NbDebugPrint(1, ("Tx pktStatus = %lx\n", inl(ioaddr + PktStatus)));
			NbDebugPrint(1, ("Tx Free = %lx\n", inw(ioaddr + TxFree)));

			NbDebugPrint(1, ("vp->tx_ring[0].addr = %lx\n", vp->tx_ring[0].addr));
			NbDebugPrint(1, ("vp->tx_ring[0].length = %lx\n", vp->tx_ring[0].length));
			NbDebugPrint(1, ("vp->tx_ring[0].status = %lx\n", vp->tx_ring[0].status));
			
			while (vp->cur_tx - dirty_tx > 0) {
				int entry = dirty_tx % TX_RING_SIZE;
				if (inl(ioaddr + DownListPtr) ==
					virt_to_bus(&vp->tx_ring[entry]))
					break;			/* It still hasn't been processed. */
				if (vp->tx_skbuff[entry]) {
					DEV_FREE_SKB(vp->tx_skbuff[entry]);
					vp->tx_skbuff[entry] = 0;
				}
				/* vp->stats.tx_packets++;  Counted below. */
				dirty_tx++;
			}
			vp->dirty_tx = dirty_tx;
			outw(AckIntr | DownComplete, ioaddr + EL3_CMD);
			if (vp->tx_full && (vp->cur_tx - dirty_tx <= TX_RING_SIZE - 1)) {
				vp->tx_full= 0;
				clear_bit(0, (void*)&dev->tbusy);
//				mark_bh(NET_BH);
			}
		}
		if (status & DMADone) {
			if (inw(ioaddr + Wn7_MasterStatus) & 0x1000) {
				outw(0x1000, ioaddr + Wn7_MasterStatus); /* Ack the event. */
				DEV_FREE_SKB(vp->tx_skb); /* Release the transfered buffer */
				if (inw(ioaddr + TxFree) > 1536) {
					clear_bit(0, (void*)&dev->tbusy);
//					mark_bh(NET_BH);
				} else /* Interrupt when FIFO has room for max-sized packet. */
					outw(SetTxThreshold + (1536>>2), ioaddr + EL3_CMD);
			}
		}
		/* Check for all uncommon interrupts at once. */
		if (status & (HostError | RxEarly | StatsFull | TxComplete | IntReq)) {
			if (status == 0xffff)
				break;
			vortex_error(dev, status);
		}

		if (--work_done < 0) {
			if ((status & (0x7fe - (UpComplete | DownComplete))) == 0) {
				/* Just ack these and return. */
				outw(AckIntr | UpComplete | DownComplete, ioaddr + EL3_CMD);
			} else {
				NbDebugPrint(1, ("%s: Too much work in interrupt, status "
					   "%4.4x.  Temporarily disabling functions (%4.4x).\n",
					   dev->name, status, SetStatusEnb | ((~status) & 0x7FE)));
				
				/* Disable all pending interrupts. */
				outw(SetStatusEnb | ((~status) & 0x7FE), ioaddr + EL3_CMD);
				outw(AckIntr | 0x7FF, ioaddr + EL3_CMD);
				/* The timer will reenable interrupts. */
				break;
			}
		}
		/* Acknowledge the IRQ. */
		outw(AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);
	} while ((status = inw(ioaddr + EL3_STATUS)) & (IntLatch | RxComplete));

	if (vortex_debug > 4)
		NbDebugPrint(1, ("%s: exiting interrupt, status %4.4x.\n",
			   dev->name, status));

#if defined(__i386__)
	clear_bit(0, (void*)&dev->interrupt);
#else
	dev->interrupt = 0;
#endif
	dev_xmit_all(NULL);	

	deltaTime_3c59x.QuadPart = - INTERRUPT_TIME;    
	KeSetTimer( &InterruptTimer_3c59x, deltaTime_3c59x, &InterruptTimerDpc_3c59x );	

	return;
}

static int vortex_rx(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int i;
	short rx_status;

	if (vortex_debug > 5) {		
		NbDebugPrint(1, ("   In rx_packet(), status %4.4x, rx_status %4.4x.\n",
			   inw(ioaddr+EL3_STATUS), inw(ioaddr+RxStatus)));
	}

	while ((rx_status = inw(ioaddr + RxStatus)) > 0) {
		if (rx_status & 0x4000) { /* Error, update stats. */
			unsigned char rx_error = inb(ioaddr + RxErrors);
			if (vortex_debug > 2) {				
				NbDebugPrint(1, (" Rx error: status %2.2x.\n", rx_error));
			}
			vp->stats.rx_errors++;
			if (rx_error & 0x01)  vp->stats.rx_over_errors++;
			if (rx_error & 0x02)  vp->stats.rx_length_errors++;
			if (rx_error & 0x04)  vp->stats.rx_frame_errors++;
			if (rx_error & 0x08)  vp->stats.rx_crc_errors++;
			if (rx_error & 0x10)  vp->stats.rx_length_errors++;
		} else {
			/* The packet length: up to 4.5K!. */
//			int pkt_len = rx_status & 0x1fff;
			unsigned int pkt_len = rx_status & 0x1fff;
			struct sk_buff *skb;

			skb = dev_alloc_skb(pkt_len + 5);
			if (vortex_debug > 4) {			
				NbDebugPrint(1, ("Receiving packet size %d status %4.4x.\n",
					   pkt_len, rx_status));
			}
			
			if (skb != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
				/* 'skb_put()' points to the start of sk_buff data area. */
				if (vp->bus_master &&
					! (inw(ioaddr + Wn7_MasterStatus) & 0x8000)) {
					outl(virt_to_bus(skb_put(skb, pkt_len)),
						 ioaddr + Wn7_MasterAddr);
					outw((skb->len + 3) & ~3, ioaddr + Wn7_MasterLen);
					outw(StartDMAUp, ioaddr + EL3_CMD);
					while (inw(ioaddr + Wn7_MasterStatus) & 0x8000)
						;
				} else {
					insl((u16)(ioaddr + RX_FIFO), (void *)skb_put(skb, pkt_len),
						 (pkt_len + 3) >> 2);
				}
				outw(RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */
				skb->protocol = eth_type_trans(skb, dev);
				netif_rx(skb);
				dev->last_rx = jiffies;
				vp->stats.rx_packets++;
				vp->stats.rx_bytes += skb->len;
				/* Wait a limited time to go to next packet. */
				NbDebugPrint(1, ("protocol = %04x\n", skb->protocol));
				
				for (i = 200; i >= 0; i--)
					if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
						break;
				continue;
			} else if (vortex_debug)				
				NbDebugPrint(1, ("%s: No memory to allocate a sk_buff of "
					   "size %d.\n", dev->name, pkt_len));
		}
		outw(RxDiscard, ioaddr + EL3_CMD);
		vp->stats.rx_dropped++;
		/* Wait a limited time to skip this packet. */
		for (i = 200; i >= 0; i--)
			if ( ! (inw(ioaddr + EL3_STATUS) & CmdInProgress))
				break;
	}

	return 0;
}

static int
boomerang_rx(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	int entry = vp->cur_rx % RX_RING_SIZE;
	long ioaddr = dev->base_addr;
	int rx_status;
	int rx_work_limit = vp->dirty_rx + RX_RING_SIZE - vp->cur_rx;
	int i;

	if (vortex_debug > 5) {		
		NbDebugPrint(1, ("  In boomerang_rx(), status %4.4x, rx_status "
			   "%4.4x.\n",
			   inw(ioaddr+EL3_STATUS), inw(ioaddr+RxStatus)));
	}
	while ((rx_status = le32_to_cpu(vp->rx_ring[entry].status)) & RxDComplete){
		if (--rx_work_limit < 0)
			break;
		if (rx_status & RxDError) { /* Error, update stats. */
			unsigned char rx_error = rx_status >> 16;
			if (vortex_debug > 2) {			
				NbDebugPrint(1, (" Rx error: status %2.2x.\n", rx_error));
			}
			vp->stats.rx_errors++;
			if (rx_error & 0x01)  vp->stats.rx_over_errors++;
			if (rx_error & 0x02)  vp->stats.rx_length_errors++;
			if (rx_error & 0x04)  vp->stats.rx_frame_errors++;
			if (rx_error & 0x08)  vp->stats.rx_crc_errors++;
			if (rx_error & 0x10)  vp->stats.rx_length_errors++;
		} else {
			/* The packet length: up to 4.5K!. */
			int pkt_len = rx_status & 0x1fff;
			struct sk_buff *skb;

			vp->stats.rx_bytes += pkt_len;
			if (vortex_debug > 4) {			
				NbDebugPrint(1, ("Receiving packet size %d status %4.4x.\n",
					   pkt_len, rx_status));
			}

			/* Check if the packet is long enough to just accept without
			   copying to a properly sized skbuff. */
			if (pkt_len < rx_copybreak
				&& (skb = dev_alloc_skb(pkt_len + 2)) != 0) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
				/* 'skb_put()' points to the start of sk_buff data area. */
				memcpy(skb_put(skb, pkt_len),
					   (PVOID)bus_to_virt(le32_to_cpu(vp->rx_ring[entry].addr)),
					   pkt_len);
				rx_copy++;
			} else {
				void *temp;
				/* Pass up the skbuff already on the Rx ring. */
				skb = vp->rx_skbuff[entry];
				vp->rx_skbuff[entry] = NULL;
				temp = skb_put(skb, pkt_len);
				/* Remove this checking code for final release. */
				if ((PVOID) bus_to_virt(le32_to_cpu(vp->rx_ring[entry].addr)) != temp) {				
					NbDebugPrint(1, ("%s: Warning -- the skbuff addresses do not match"
						   " in boomerang_rx: %p vs. %p.\n", dev->name,
						   bus_to_virt(le32_to_cpu(vp->rx_ring[entry].addr)),
						   temp));
				}
				rx_nocopy++;
			}
			skb->protocol = eth_type_trans(skb, dev);
			NbDebugPrint(1, ("ether type = %lx\n", HTONS(skb->protocol))) ;

		/*	for(i=0;i<pkt_len;i++) {
				NbDebugPrint(1, ("%02x:", skb->data[i]));
				if((i+1) % 0x10 == 0) NbDebugPrint(1, ("\n"));
			}
			NbDebugPrint(1, ("\n"));
		*/
			
			{					/* Use hardware checksum info. */
				int csum_bits = rx_status & 0xee000000;
				if (csum_bits &&
					(csum_bits == (IPChksumValid | TCPChksumValid) ||
					 csum_bits == (IPChksumValid | UDPChksumValid))) {
					skb->ip_summed = CHECKSUM_UNNECESSARY;
					rx_csumhits++;
				}
			}
			netif_rx(skb);
			dev->last_rx = jiffies;
			vp->stats.rx_packets++;
		}
		entry = (++vp->cur_rx) % RX_RING_SIZE;
	}
	/* Refill the Rx ring buffers. */
	for (; vp->dirty_rx < vp->cur_rx; vp->dirty_rx++) {
		struct sk_buff *skb;
		entry = vp->dirty_rx % RX_RING_SIZE;
		if (vp->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(PKT_BUF_SZ);
			if (skb == NULL)
				break;			/* Bad news!  */
			skb->dev = dev;			/* Mark as being used by this device. */
			skb_reserve(skb, 2);	/* Align IP on 16 byte boundaries */
			vp->rx_ring[entry].addr = cpu_to_le32(virt_to_bus(skb->tail));
			vp->rx_skbuff[entry] = skb;
		}
		vp->rx_ring[entry].status = 0;	/* Clear complete bit. */
		outw(UpUnstall, ioaddr + EL3_CMD);
	}
	return 0;
}

static int
vortex_close(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;
	int i;

	if(!dev->start) return 0;

	dev->start = 0;
	dev->tbusy = 1;

	if (vortex_debug > 1) {
		NbDebugPrint(0, ("%s: vortex_close() status %4.4x, Tx status %2.2x.\n",
			   dev->name, inw(ioaddr + EL3_STATUS), inb(ioaddr + TxStatus)));
		NbDebugPrint(0, ( "%s: vortex close stats: rx_nocopy %d rx_copy %d"
			   " tx_queued %d Rx pre-checksummed %d.\n",
			   dev->name, rx_nocopy, rx_copy, queued_packet, rx_csumhits));
	}

//	KeCancelTimer(&Timer_3c59x);
	KeCancelTimer(&InterruptTimer_3c59x);

	/* Turn off statistics ASAP.  We update vp->stats below. */
	outw(StatsDisable, ioaddr + EL3_CMD);

	/* Disable the receiver and transmitter. */
	outw(RxDisable, ioaddr + EL3_CMD);
	outw(TxDisable, ioaddr + EL3_CMD);

	if (dev->if_port == XCVR_10base2)
		/* Turn off thinnet power.  Green! */
		outw(StopCoax, ioaddr + EL3_CMD);	

	outw(SetIntrEnb | 0x0000, ioaddr + EL3_CMD);

	update_stats(ioaddr, dev);
	if (vp->full_bus_master_rx) { /* Free Boomerang bus master Rx buffers. */
		outl(0, ioaddr + UpListPtr);
		for (i = 0; i < RX_RING_SIZE; i++)
			if (vp->rx_skbuff[i]) {
				DEV_FREE_SKB(vp->rx_skbuff[i]);
				vp->rx_skbuff[i] = 0;
			}
	}
	if (vp->full_bus_master_tx) { /* Free Boomerang bus master Tx buffers. */
		outl(0, ioaddr + DownListPtr);
		for (i = 0; i < TX_RING_SIZE; i++)
			if (vp->tx_skbuff[i]) {
				DEV_FREE_SKB(vp->tx_skbuff[i]);
				vp->tx_skbuff[i] = 0;
			}
	}

	return 0;
}

static struct net_device_stats *vortex_get_stats(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	unsigned long flags;

	if (dev->start) {
		update_stats(dev->base_addr, dev);
	}
	return &vp->stats;
}

static int
vortex_get_status(struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;
	long ioaddr = dev->base_addr;

	if( inl(ioaddr + UpListPtr) != (ULONG)virt_to_bus(&vp->rx_ring[0]) ) {
		NbDebugPrint (0, ("3com59x status is corrupted !!!!!!!\n"));
		return STATUS_NIC_CORRUPTED;
	}	

	return STATUS_NIC_OK;
}

/*  Update statistics.
	Unlike with the EL3 we need not worry about interrupts changing
	the window setting from underneath us, but we must still guard
	against a race condition with a StatsUpdate interrupt updating the
	table.  This is done by checking that the ASM (!) code generated uses
	atomic updates with '+='.
	*/
static void update_stats(long ioaddr, struct device *dev)
{
	struct vortex_private *vp = (struct vortex_private *)dev->priv;

	/* Unlike the 3c5x9 we need not turn off stats updates while reading. */
	/* Switch to the stats window, and read everything. */
	EL3WINDOW(6);
	vp->stats.tx_carrier_errors		+= inb(ioaddr + 0);
	vp->stats.tx_heartbeat_errors	+= inb(ioaddr + 1);
	/* Multiple collisions. */		inb(ioaddr + 2);
	vp->stats.collisions			+= inb(ioaddr + 3);
	vp->stats.tx_window_errors		+= inb(ioaddr + 4);
	vp->stats.rx_fifo_errors		+= inb(ioaddr + 5);
	vp->stats.tx_packets			+= inb(ioaddr + 6);
	vp->stats.tx_packets			+= (inb(ioaddr + 9)&0x30) << 4;
	/* Rx packets	*/				inb(ioaddr + 7);   /* Must read to clear */
	/* Tx deferrals */				inb(ioaddr + 8);
	/* Don't bother with register 9, an extension of registers 6&7.
	   If we do use the 6&7 values the atomic update assumption above
	   is invalid. */
	inw(ioaddr + 10);	/* Total Rx and Tx octets. */
	inw(ioaddr + 12);
	/* New: On the Vortex we must also clear the BadSSD counter. */
	EL3WINDOW(4);
	inb(ioaddr + 12);

	/* We change back to window 7 (not 1) with the Vortex. */
	EL3WINDOW(7);
	return;
}

/* Pre-Cyclone chips have no documented multicast filter, so the only
   multicast setting is to receive all multicast frames.  At least
   the chip has a very clean way to set the mode, unlike many others. */
static void set_rx_mode(struct device *dev)
{
	long ioaddr = dev->base_addr;
	int new_mode;

	if (dev->flags & IFF_PROMISC) {
		if (vortex_debug > 0)
			NbDebugPrint(0, ( "%s: Setting promiscuous mode.\n", dev->name));
		new_mode = SetRxFilter|RxStation|RxMulticast|RxBroadcast|RxProm;
	} else	if ((dev->mc_list)  ||  (dev->flags & IFF_ALLMULTI)) {
		new_mode = SetRxFilter|RxStation|RxMulticast|RxBroadcast;
	} else
		new_mode = SetRxFilter | RxStation | RxBroadcast;

	outw(new_mode, ioaddr + EL3_CMD);
}


/* MII transceiver control section.
   Read and write the MII registers using software-generated serial
   MDIO protocol.  See the MII specifications or DP83840A data sheet
   for details. */

/* The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
   met by back-to-back PCI I/O cycles, but we insert a delay to avoid
   "overclocking" issues. */

__inline void mdio_delay() 
{
#ifndef __ENABLE_LOADER__
	LARGE_INTEGER	interval;

	interval.QuadPart = - 1;
//	KeDelayExecutionThread(KernelMode, FALSE, &interval);
	KeDelayExecution(50);
#endif

	return;
}

#define MDIO_SHIFT_CLK	0x01
#define MDIO_DIR_WRITE	0x04
#define MDIO_DATA_WRITE0 (0x00 | MDIO_DIR_WRITE)
#define MDIO_DATA_WRITE1 (0x02 | MDIO_DIR_WRITE)
#define MDIO_DATA_READ	0x02
#define MDIO_ENB_IN		0x00

/* Generate the preamble required for initial synchronization and
   a few older transceivers. */
static void mdio_sync(long ioaddr, int bits)
{
	long mdio_addr = ioaddr + Wn4_PhysicalMgmt;

	/* Establish sync by sending at least 32 logic ones. */
	while (-- bits >= 0) {
		outw(MDIO_DATA_WRITE1, mdio_addr);
		mdio_delay();
		outw(MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
}

static int mdio_read(long ioaddr, int phy_id, int location)
{
	int i;
	int read_cmd = (0xf6 << 10) | (phy_id << 5) | location;
	unsigned int retval = 0;
	long mdio_addr = ioaddr + Wn4_PhysicalMgmt;

	if (mii_preamble_required)
		mdio_sync(ioaddr, 32);

	/* Shift the read command bits out. */
	for (i = 14; i >= 0; i--) {
		int dataval = (read_cmd&(1<<i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
		outw(dataval, mdio_addr);
		mdio_delay();
		outw(dataval | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	/* Read the two transition, 16 data, and wire-idle bits. */
	for (i = 19; i > 0; i--) {
		outw(MDIO_ENB_IN, mdio_addr);
		mdio_delay();
		retval = (retval << 1) | ((inw(mdio_addr) & MDIO_DATA_READ) ? 1 : 0);
		outw(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
#if 0
	return (retval>>1) & 0x1ffff;
#else
	return retval & 0x20000 ? 0xffff : retval>>1 & 0xffff;
#endif
}

static void mdio_write(long ioaddr, int phy_id, int location, int value)
{
	int write_cmd = 0x50020000 | (phy_id << 23) | (location << 18) | value;
	long mdio_addr = ioaddr + Wn4_PhysicalMgmt;
	int i;

	if (mii_preamble_required)
		mdio_sync(ioaddr, 32);

	/* Shift the command bits out. */
	for (i = 31; i >= 0; i--) {
		int dataval = (write_cmd&(1<<i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
		outw(dataval, mdio_addr);
		mdio_delay();
		outw(dataval | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}
	/* Leave the interface idle. */
	for (i = 1; i >= 0; i--) {
		outw(MDIO_ENB_IN, mdio_addr);
		mdio_delay();
		outw(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
		mdio_delay();
	}

	return;
}


/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -c 3c59x.c `[ -f /usr/include/linux/modversions.h ] && echo -DMODVERSIONS`"
 *  SMP-compile-command: "gcc -D__SMP__ -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -c 3c59x.c"
 *  cardbus-compile-command: "gcc -DCARDBUS -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -c 3c59x.c -o 3c575_cb.o -I/usr/src/pcmcia-cs-3.0.5/include/"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */