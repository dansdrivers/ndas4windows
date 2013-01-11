/******************************************************************************
 *
 * Name:        skge.c
 * Project:     GEnesis, PCI Gigabit Ethernet Adapter
 * Version:     $Revision: 1.60.2.63 $
 * Date:        $Date: 2005/09/29 08:09:47 $
 * Purpose:     The main driver source module
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	(C)Copyright 1998-2002 SysKonnect GmbH.
 *	(C)Copyright 2002-2005 Marvell.
 *
 *	Driver for Marvell Yukon chipset and SysKonnect Gigabit Ethernet 
 *      Server Adapters.
 *
 *	Author: Mirko Lindner (mlindner@syskonnect.de)
 *	        Ralph Roesler (rroesler@syskonnect.de)
 *
 *	Address all question to: linux@syskonnect.de
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Description:
 *
 *	All source files in this sk98lin directory except of the sk98lin 
 *	Linux specific files
 *
 *		- skdim.c
 *		- skethtool.c
 *		- skge.c
 *		- skproc.c
 *		- sky2.c
 *		- Makefile
 *		- h/skdrv1st.h
 *		- h/skdrv2nd.h
 *		- h/sktypes.h
 *		- h/skversion.h
 *
 *	are part of SysKonnect's common modules for the SK-9xxx adapters.
 *
 *	Those common module files which are not Linux specific are used to 
 *	build drivers on different OS' (e.g. Windows, MAC OS) so that those
 *	drivers are based on the same set of files
 *
 *	At a first glance, this seems to complicate things unnescessarily on 
 *	Linux, but please do not try to 'clean up' them without VERY good 
 *	reasons, because this will make it more difficult to keep the sk98lin
 *	driver for Linux in synchronisation with the other drivers running on
 *	other operating systems.
 *
 ******************************************************************************/
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
#include "pci.h"
#include "nic.h"
#include "endian.h"

#include	"h/skversion.h"
#include	"h/skdrv1st.h"
#include	"h/skdrv2nd.h"


/*******************************************************************************
 *
 * Defines
 *
 ******************************************************************************/

#define		MOD_INC_USE_COUNT
#define		MOD_DEC_USE_COUNT


/* for debuging on x86 only */
/* #define BREAKPOINT() asm(" int $3"); */


/* Set blink mode*/
#define OEM_CONFIG_VALUE (	SK_ACT_LED_BLINK | \
				SK_DUP_LED_NORMAL | \
				SK_LED_LINK100_ON)

#define CLEAR_AND_START_RX(Port) SK_OUT8(pAC->IoBase, RxQueueAddr[(Port)]+Q_CSR, CSR_START | CSR_IRQ_CL_F)
#define CLEAR_TX_IRQ(Port,Prio) SK_OUT8(pAC->IoBase, TxQueueAddr[(Port)][(Prio)]+Q_CSR, CSR_IRQ_CL_F)

KDPC SkGeInterruptTimerDpc;
KTIMER SkGeInterruptTimer;

/*******************************************************************************
 *
 * Local Function Prototypes
 *
 ******************************************************************************/

int __init skge_probe (struct SK_NET_DEVICE *dev);
static void 	sk98lin_remove_device(struct pci_dev *pdev);
#ifdef CONFIG_PM
static int	sk98lin_suspend(struct pci_dev *pdev, u32 state);
static int	sk98lin_resume(struct pci_dev *pdev);
static void	SkEnableWOMagicPacket(SK_AC *pAC, SK_IOC IoC, SK_MAC_ADDR MacAddr);
#endif
#ifdef Y2_RECOVERY
//static void	SkGeHandleKernelTimer(unsigned long ptr);
static void SkGeHandleKernelTimer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);
void		SkGeCheckTimer(DEV_NET *pNet);
static SK_BOOL  CheckRXCounters(DEV_NET *pNet);
static void	CheckRxPath(DEV_NET *pNet);
#endif
static void	FreeResources(struct SK_NET_DEVICE *dev);
static int	SkGeBoardInit(struct SK_NET_DEVICE *dev, SK_AC *pAC);
static SK_BOOL	BoardAllocMem(SK_AC *pAC);
static void	BoardFreeMem(SK_AC *pAC);
static void	BoardInitMem(SK_AC *pAC);
static void	SetupRing(SK_AC*, void*, uintptr_t2, RXD**, RXD**, RXD**, int*, int*, SK_BOOL);
//static SkIsrRetVar	SkGeIsr(int irq, void *dev_id, struct pt_regs *ptregs);
static void SkGeIsr(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);
//static SkIsrRetVar	SkGeIsrOnePort(int irq, void *dev_id, struct pt_regs *ptregs);
static void SkGeIsrOnePort(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);

static int	SkGeOpen(struct SK_NET_DEVICE *dev);
static int	SkGeClose(struct SK_NET_DEVICE *dev);
static int	SkGeXmit(struct sk_buff *skb, struct SK_NET_DEVICE *dev);
static int	SkGeSetMacAddr(struct SK_NET_DEVICE *dev, void *p);
static int SkGe_get_status(struct SK_NET_DEVICE *dev);
static void	SkGeSetRxMode(struct SK_NET_DEVICE *dev);
static struct	net_device_stats *SkGeStats(struct SK_NET_DEVICE *dev);
static int	SkGeIoctl(struct SK_NET_DEVICE *dev, struct ifreq *rq, int cmd);
static void	GetConfiguration(SK_AC*);
static void	ProductStr(SK_AC*);
static int	XmitFrame(SK_AC*, TX_PORT*, struct sk_buff*);
static void	FreeTxDescriptors(SK_AC*pAC, TX_PORT*);
static void	FillRxRing(SK_AC*, RX_PORT*);
static SK_BOOL	FillRxDescriptor(SK_AC*, RX_PORT*);
#ifdef CONFIG_SK98LIN_NAPI
static int	SkGePoll(struct net_device *dev, int *budget);
static void	ReceiveIrq(SK_AC*, RX_PORT*, SK_BOOL, int*, int);
#else
static void	ReceiveIrq(SK_AC*, RX_PORT*, SK_BOOL);
#endif
#ifdef SK_POLL_CONTROLLER
static void	SkGeNetPoll(struct SK_NET_DEVICE *dev);
#endif
static void	ClearRxRing(SK_AC*, RX_PORT*);
static void	ClearTxRing(SK_AC*, TX_PORT*);
static int	SkGeChangeMtu(struct SK_NET_DEVICE *dev, int new_mtu);
static void	PortReInitBmu(SK_AC*, int);
static int	SkGeIocMib(DEV_NET*, unsigned int, int);
static int	SkGeInitPCI(SK_AC *pAC);
static SK_U32   ParseDeviceNbrFromSlotName(const char *SlotName);
static int      SkDrvInitAdapter(SK_AC *pAC, int devNbr);
static int      SkDrvDeInitAdapter(SK_AC *pAC, int devNbr);
extern void	SkLocalEventQueue(	SK_AC *pAC,
					SK_U32 Class,
					SK_U32 Event,
					SK_U32 Param1,
					SK_U32 Param2,
					SK_BOOL Flag);
extern void	SkLocalEventQueue64(	SK_AC *pAC,
					SK_U32 Class,
					SK_U32 Event,
					SK_U64 Param,
					SK_BOOL Flag);

static int	XmitFrameSG(SK_AC*, TX_PORT*, struct sk_buff*);

/*******************************************************************************
 *
 * Extern Function Prototypes
 *
 ******************************************************************************/

extern SK_BOOL SkY2AllocateResources(SK_AC *pAC);
extern void SkY2FreeResources(SK_AC *pAC);
extern void SkY2AllocateRxBuffers(SK_AC *pAC,SK_IOC IoC,int Port);
extern void SkY2FreeRxBuffers(SK_AC *pAC,SK_IOC IoC,int Port);
extern void SkY2FreeTxBuffers(SK_AC *pAC,SK_IOC IoC,int Port);
//extern SkIsrRetVar SkY2Isr(int irq,void *dev_id,struct pt_regs *ptregs);
extern void SkY2Isr(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
);
extern int SkY2Xmit(struct sk_buff *skb,struct SK_NET_DEVICE *dev);
extern void SkY2PortStop(SK_AC *pAC,SK_IOC IoC,int Port,int Dir,int RstMode);
extern void SkY2PortStart(SK_AC *pAC,SK_IOC IoC,int Port);
extern int SkY2RlmtSend(SK_AC *pAC,int PortNr,struct sk_buff *pMessage);
extern void SkY2RestartStatusUnit(SK_AC *pAC);
extern void FillReceiveTableYukon2(SK_AC *pAC,SK_IOC IoC,int Port);
#ifdef CONFIG_SK98LIN_NAPI
extern int SkY2Poll(struct net_device *dev, int *budget);
#endif

extern void SkDimEnableModerationIfNeeded(SK_AC *pAC);	
extern void SkDimStartModerationTimer(SK_AC *pAC);
extern void SkDimModerate(SK_AC *pAC);

extern int SkEthIoctl(struct net_device *netdev, struct ifreq *ifr);

#ifdef CONFIG_PROC_FS
static const char 	SK_Root_Dir_entry[] = "sk98lin";
static struct		proc_dir_entry *pSkRootDir;
extern int 	sk_proc_read(	char   *buffer,
				char	**buffer_location,
				off_t	offset,
				int	buffer_length,
				int	*eof,
				void	*data);
#endif

#ifdef DEBUG
static void	DumpMsg(struct sk_buff*, char*);
static void	DumpData(char*, int);
static void	DumpLong(char*, int);
static void skge_dump_state(SK_AC *pAC);
#endif

/* global variables *********************************************************/
static const char *BootString = BOOT_STRING;
struct SK_NET_DEVICE *SkGeRootDev = NULL;
static SK_BOOL DoPrintInterfaceChange = SK_TRUE;

/* local variables **********************************************************/
static uintptr_t2 TxQueueAddr[SK_MAX_MACS][2] = {{0x680, 0x600},{0x780, 0x700}};
static uintptr_t2 RxQueueAddr[SK_MAX_MACS] = {0x400, 0x480};
static int sk98lin_max_boards_found = 0;

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry	*pSkRootDir;
#endif



static struct pci_device_id sk98lin_pci_tbl[] __devinitdata = {
/*	{ pci_vendor_id, pci_device_id, * SAMPLE ENTRY! *
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL }, */
	{ 0x10b7, 0x1700, /* 3Com (10b7), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x10b7, 0x80eb, /* 3Com (10b7), 3Com 3C940B Gigabit LOM Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x4300, /* SysKonnect (1148), SK-98xx Gigabit Ethernet Server Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x4320, /* SysKonnect (1148), SK-98xx V2.0 Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x9000, /* SysKonnect (1148), SK-9Sxx 10/100/1000Base-T Server Adapter  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1148, 0x9E00, /* SysKonnect (1148), SK-9Exx 10/100/1000Base-T Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4b00, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4b01, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1186, 0x4c00, /* D-Link (1186), Gigabit Ethernet Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4320, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4340, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4341, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4342, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4343, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4344, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4345, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4346, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4347, /* Marvell (11ab), Gigabit Ethernet Controller  */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4350, /* Marvell (11ab), Fast Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4351, /* Marvell (11ab), Fast Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4352, /* Marvell (11ab), Fast Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4360, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4361, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4362, /* Marvell (11ab), Gigabit Ethernet Controller */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x4363, /* Marvell (11ab), Marvell */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x11ab, 0x5005, /* Marvell (11ab), Belkin */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1371, 0x434e, /* CNet (1371), GigaCard Network Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1737, 0x1032, /* Linksys (1737), Gigabit Network Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0x1737, 0x1064, /* Linksys (1737), Gigabit Network Adapter */
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ 0, }
};


/*****************************************************************************
 *
 * 	sk98lin_init_device - initialize the adapter
 *
 * Description:
 *	This function initializes the adapter. Resources for
 *	the adapter are allocated and the adapter is brought into Init 1
 *	state.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
int __init skge_probe (struct SK_NET_DEVICE *dev)
{
	static SK_BOOL 	sk98lin_boot_string = SK_FALSE;
	static SK_BOOL 	sk98lin_proc_entry = SK_FALSE;
	static int		sk98lin_boards_found = 0;
	int				vendor_flag = SK_FALSE;
	SK_AC			*pAC;
	DEV_NET			*pNet = NULL;	
	struct pci_dev	*pdev = NULL;	
	int			retval;
#ifdef CONFIG_PROC_FS
	int			proc_root_initialized = 0;
	struct proc_dir_entry	*pProcFile;
#endif

	NbDebugPrint(0, ("skge_probe Entered ~~~~~\n"));

	while((pdev = pci_find_class(PCI_CLASS_NETWORK_ETHERNET << 8, pdev))) {

		retval = pci_enable_device(pdev);
		if (retval) {
			NbDebugPrint(0, ("Cannot enable PCI device, "
				"aborting.\n"));
			return retval;
		}

		NbDebugPrint(0, ( "pci device = %p, pdev->vendor = %04X, pdev->device = %04X\n", pdev, pdev->vendor, pdev->device));

//		read_config_header(pdev);
//		display_config_header(pdev);

		SK_PCI_ISCOMPLIANT(vendor_flag, pdev);
		if (!vendor_flag)
			continue;
		
		
//		dev = NULL;
		pNet = NULL;


		/* INSERT * We have to find the power-management capabilities */
		/* Find power-management capability. */



		/* Configure DMA attributes. */
		retval = pci_set_dma_mask(pdev, (u64) 0xffffffffffffffffULL);
		if (!retval) {
			retval = pci_set_dma_mask(pdev, (u64) 0xffffffff);
			if (retval)
				return retval;
		} else {
			return retval;
		}

	/*
		if ((dev = init_etherdev(dev, sizeof(DEV_NET))) == NULL) {
			NbDebugPrint(0, ("Unable to allocate etherdev "
				"structure!\n")));
			return -ENODEV;
		}
	*/
	//	memset(dev, 0, sizeof(DEV_NET));
		/* Some data structures must be quadword aligned. */
		pNet = kmalloc(sizeof(*pNet), 0);
		memset(pNet, 0, sizeof(*pNet));
		dev->priv = pNet;
		
		pNet->pAC = kmalloc(sizeof(SK_AC), GFP_KERNEL);
//		pNet->pAC = kmalloc(sizeof(SK_AC) + 8191, GFP_KERNEL);
//		pNet->pAC = (SK_AC *)(((ULONG)pNet->pAC + 8191) & ~8191);

		if (pNet->pAC == NULL){			
			dev->get_stats = NULL;
			kfree(dev->priv);
			NbDebugPrint(0, ("Unable to allocate adapter "
				"structure!\n"));
			return -ENODEV;
		}


		/* Print message */
		if (!sk98lin_boot_string) {
			/* set display flag to TRUE so that */
			/* we only display this string ONCE */
			sk98lin_boot_string = SK_TRUE;
			NbDebugPrint(0, ("%s\n", BootString));
		}

		memset(pNet->pAC, 0, sizeof(SK_AC));
		pAC = pNet->pAC;
		pAC->PciDev = pdev;
		pAC->PciDevId = pdev->device;
		pAC->dev[0] = dev;
		pAC->dev[1] = dev;
		sprintf(pAC->Name, "SysKonnect SK-98xx");
		pAC->CheckQueue = SK_FALSE;

		dev->irq = pdev->irq;
		retval = SkGeInitPCI(pAC);
		if (retval) {
			NbDebugPrint(0, ("SKGE: PCI setup failed: %i\n", retval));
			dev->get_stats = NULL;
//			kfree(dev);
			return -ENODEV;
		}


		dev->open		=  &SkGeOpen;
		dev->stop		=  &SkGeClose;
		dev->get_stats		=  &SkGeStats;
		dev->get_status = &SkGe_get_status;
//		dev->set_multicast_list	=  &SkGeSetRxMode;
//		dev->set_mac_address	=  &SkGeSetMacAddr;
//		dev->do_ioctl		=  &SkGeIoctl;
//		dev->change_mtu		=  &SkGeChangeMtu;
		dev->flags		&= ~IFF_RUNNING;
	#ifdef SK_POLL_CONTROLLER
		dev->poll_controller	=  SkGeNetPoll;
	#endif

		pAC->Index = sk98lin_boards_found;

		if (SkGeBoardInit(dev, pAC)) {
//			kfree(dev);
			return -ENODEV;
		} else {
			ProductStr(pAC);
		}

		/* shifter to later moment in time... */
		if (CHIP_ID_YUKON_2(pAC)) {
			dev->hard_start_xmit =	&SkY2Xmit;
	#ifdef CONFIG_SK98LIN_NAPI
			dev->poll =  &SkY2Poll;
			dev->weight = 64;
	#endif
		} else {
			dev->hard_start_xmit =	&SkGeXmit;
	#ifdef CONFIG_SK98LIN_NAPI
			dev->poll =  &SkGePoll;
			dev->weight = 64;
	#endif
		}

	#ifdef NETIF_F_TSO
	#ifdef USE_SK_TSO_FEATURE	
		if ((CHIP_ID_YUKON_2(pAC)) && 
			(pAC->GIni.GIChipId != CHIP_ID_YUKON_EC_U)) {
			dev->features |= NETIF_F_TSO;
		}
	#endif
	#endif
	#ifdef CONFIG_SK98LIN_ZEROCOPY
		if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
			dev->features |= NETIF_F_SG;
	#endif
	#ifdef USE_SK_TX_CHECKSUM
		if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
			dev->features |= NETIF_F_IP_CSUM;
	#endif
	#ifdef USE_SK_RX_CHECKSUM
		pAC->RxPort[0].UseRxCsum = SK_TRUE;
		if (pAC->GIni.GIMacsFound == 2 ) {
			pAC->RxPort[1].UseRxCsum = SK_TRUE;
		}
	#endif

		/* Save the hardware revision */
		pAC->HWRevision = (((pAC->GIni.GIPciHwRev >> 4) & 0x0F)*10) +
			(pAC->GIni.GIPciHwRev & 0x0F);

		NbDebugPrint(3, ("pAC->HWRevision = %08X\n", pAC->HWRevision));
		/* Set driver globals */
		pAC->Pnmi.pDriverFileName    = DRIVER_FILE_NAME;
		pAC->Pnmi.pDriverReleaseDate = DRIVER_REL_DATE;

		SK_MEMSET(&(pAC->PnmiBackup), 0, sizeof(SK_PNMI_STRUCT_DATA));
		SK_MEMCPY(&(pAC->PnmiBackup), &(pAC->PnmiStruct), 
				sizeof(SK_PNMI_STRUCT_DATA));

		{
			SK_U16 PciPMControlStatus, PciPMCapabilities, PciPMData;
			/* read the PM control/status register from the PCI config space */
			SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CTL_STS), &PciPMControlStatus);
			SK_OUT16(pAC->IoBase, PCI_C(pAC, PCI_PM_CTL_STS), PciPMControlStatus | 0xE00);

			SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CTL_STS), &PciPMControlStatus);
			NbDebugPrint(3, ("PCI_C(pAC, PCI_PM_CTL_STS) = %08X\n", PciPMControlStatus));

			SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_DAT_REG), &PciPMData);
			NbDebugPrint(3, ("before PCI_C(pAC, PCI_PM_DAT_REG) = %08X\n", PciPMData));

			SK_OUT16(pAC->IoBase, PCI_C(pAC, PCI_PM_DAT_REG), 0x0C00);
			SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_DAT_REG), &PciPMData);
			NbDebugPrint(3, ("after PCI_C(pAC, PCI_PM_DAT_REG) = %08X\n", PciPMData));

			SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CTL_STS), &PciPMControlStatus);
			NbDebugPrint(3, ("PCI_C(pAC, PCI_PM_CTL_STS) = %08X\n", PciPMControlStatus));

			/* read the power management capabilities from the config space */
			SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CAP_REG), &PciPMCapabilities);
			NbDebugPrint(3, ("PCI_C(pAC, PCI_PM_CAP_REG) = %08X\n", PciPMCapabilities));

		}

		/* Save initial device name */
		strcpy(pNet->InitialDevName, dev->name);
		

		/* Set network to off */
		netif_stop_queue(dev);
		netif_carrier_off(dev);

		/* Print adapter specific string from vpd and config settings */
		NbDebugPrint(0, ("%s: %s\n", pNet->InitialDevName, pAC->DeviceStr));
		NbDebugPrint(0, ("      PrefPort:%c  RlmtMode:%s\n",
			'A' + pAC->Rlmt.Net[0].Port[pAC->Rlmt.Net[0].PrefPort]->PortNumber,
			(pAC->RlmtMode==0)  ? "Check Link State" :
			((pAC->RlmtMode==1) ? "Check Link State" :
			((pAC->RlmtMode==3) ? "Check Local Port" :
			((pAC->RlmtMode==7) ? "Check Segmentation" :
			((pAC->RlmtMode==17) ? "Dual Check Link State" :"Error"))))));

		SkGeYellowLED(pAC, pAC->IoBase, 1);

		memcpy((caddr_t) &dev->dev_addr,
			(caddr_t) &pAC->Addr.Net[0].CurrentMacAddress, 6);

		/* First adapter... Create proc and print message */
	#ifdef CONFIG_PROC_FS
		if (!sk98lin_proc_entry) {
			sk98lin_proc_entry = SK_TRUE;
			SK_MEMCPY(&SK_Root_Dir_entry, BootString,
				sizeof(SK_Root_Dir_entry) - 1);

			/*Create proc (directory)*/
			if(!proc_root_initialized) {
				pSkRootDir = create_proc_entry(SK_Root_Dir_entry,
				S_IFDIR | S_IRUGO | S_IXUGO, proc_net);
				pSkRootDir->owner = THIS_MODULE;
				proc_root_initialized = 1;
			}
		}

		/* Create proc file */
		pProcFile = create_proc_entry(dev->name,
			S_IFREG | S_IRUSR | S_IRGRP | S_IROTH,
			pSkRootDir);

		pProcFile->read_proc   = sk_proc_read;
		pProcFile->write_proc  = NULL;
		pProcFile->nlink       = 1;
		pProcFile->size        = sizeof(dev->name + 1);
		pProcFile->data        = (void *)pProcFile;
		pProcFile->owner       = THIS_MODULE;
	#endif

		pNet->PortNr = 0;
		pNet->NetNr  = 0;

		sk98lin_boards_found++;
		pci_set_drvdata(pdev, dev);

		/* More then one port found */
		if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
/*
			if ((dev = init_etherdev(NULL, sizeof(DEV_NET))) == 0) {
				NbDebugPrint(0, ("Unable to allocate etherdev structure!\n"));
				return -ENODEV;
			}
*/
			pAC->dev[1]   = dev;
			pNet          = dev->priv;
			pNet->PortNr  = 1;
			pNet->NetNr   = 1;
			pNet->pAC     = pAC;

			if (CHIP_ID_YUKON_2(pAC)) {
				dev->hard_start_xmit = &SkY2Xmit;
	#ifdef CONFIG_SK98LIN_NAPI
				dev->poll =  &SkY2Poll;
				dev->weight = 64;
	#endif
			} else {
				dev->hard_start_xmit = &SkGeXmit;
	#ifdef CONFIG_SK98LIN_NAPI
				dev->poll =  &SkGePoll;
				dev->weight = 64;
	#endif
			}
			dev->open               = &SkGeOpen;
			dev->stop               = &SkGeClose;
			dev->get_stats          = &SkGeStats;
			dev->get_status			= &SkGe_get_status;
//			dev->set_multicast_list = &SkGeSetRxMode;
//			dev->set_mac_address    = &SkGeSetMacAddr;
//			dev->do_ioctl           = &SkGeIoctl;
//			dev->change_mtu         = &SkGeChangeMtu;
			dev->flags             &= ~IFF_RUNNING;
	#ifdef SK_POLL_CONTROLLER
			dev->poll_controller	= SkGeNetPoll;
	#endif

	#ifdef NETIF_F_TSO
	#ifdef USE_SK_TSO_FEATURE	
			if ((CHIP_ID_YUKON_2(pAC)) && 
				(pAC->GIni.GIChipId != CHIP_ID_YUKON_EC_U)) {
				dev->features |= NETIF_F_TSO;
			}
	#endif
	#endif
	#ifdef CONFIG_SK98LIN_ZEROCOPY
			/* Don't handle if Genesis chipset */
			if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
				dev->features |= NETIF_F_SG;
	#endif
	#ifdef USE_SK_TX_CHECKSUM
			/* Don't handle if Genesis chipset */
			if (pAC->GIni.GIChipId != CHIP_ID_GENESIS)
				dev->features |= NETIF_F_IP_CSUM;
	#endif


			/* Save initial device name */
			strcpy(pNet->InitialDevName, dev->name);

			/* Set network to off */
			netif_stop_queue(dev);
			netif_carrier_off(dev);


	#ifdef CONFIG_PROC_FS
			pProcFile = create_proc_entry(dev->name,
				S_IFREG | S_IRUSR | S_IRGRP | S_IROTH,
				pSkRootDir);
			pProcFile->read_proc  = sk_proc_read;
			pProcFile->write_proc = NULL;
			pProcFile->nlink      = 1;
			pProcFile->size       = sizeof(dev->name + 1);
			pProcFile->data       = (void *)pProcFile;
			pProcFile->owner      = THIS_MODULE;
	#endif

			memcpy((caddr_t) &dev->dev_addr,
			(caddr_t) &pAC->Addr.Net[1].CurrentMacAddress, 6);
		
			NbDebugPrint(0, ("%s: %s\n", pNet->InitialDevName, pAC->DeviceStr));
			NbDebugPrint(0, ("      PrefPort:B  RlmtMode:Dual Check Link State\n"));
		}
		pAC->Index = sk98lin_boards_found;
	}
	
	sk98lin_max_boards_found = sk98lin_boards_found;

	return sk98lin_boards_found ? 0 : -ENODEV;


}


extern PVOID		HwExtension;
/*****************************************************************************
 *
 * 	SkGeInitPCI - Init the PCI resources
 *
 * Description:
 *	This function initialize the PCI resources and IO
 *
 * Returns: N/A
 *	
 */
int SkGeInitPCI(SK_AC *pAC)
{
	struct SK_NET_DEVICE *dev = pAC->dev[0];
	struct pci_dev *pdev = pAC->PciDev;
	int retval;

	if (pci_enable_device(pdev) != 0) {
		return 1;
	}

	dev->mem_start = pci_resource_start (pdev, 0);
	pci_set_master(pdev);

//	if (pci_request_regions(pdev, DRIVER_FILE_NAME) != 0) {
//		retval = 2;
//		goto out_disable;
//	}

#ifdef SK_BIG_ENDIAN
	/*
	 * On big endian machines, we use the adapter's aibility of
	 * reading the descriptors as big endian.
	 */
	{
		SK_U32		our2;
		SkPciReadCfgDWord(pAC, PCI_OUR_REG_2, &our2);
		our2 |= PCI_REV_DESC;
		SkPciWriteCfgDWord(pAC, PCI_OUR_REG_2, our2);
	}
#endif

	/*
	 * Remap the regs into kernel space.
	 */
//	pAC->IoBase = (char*)ioremap_nocache(dev->mem_start, 0x4000);
#ifdef __ENABLE_LOADER__
	pAC->IoBase = ScsiPortGetDeviceBase(NULL,
                                        PCIBus,
										pdev->bus->number,
                                        ScsiPortConvertUlongToPhysicalAddress(dev->mem_start),
                                        0x4000,
                                        FALSE);
#else

	pAC->IoBase = ScsiPortGetDeviceBase(HwExtension,
                                        PCIBus,
										pdev->bus->number,
                                        ScsiPortConvertUlongToPhysicalAddress(dev->mem_start),
                                        0x4000,
                                        FALSE);
#endif	

	NbDebugPrint(0, ("pAC->IoBase = %08X, dev->mem_start = %08X, physizcal address = %08X\n", pAC->IoBase, dev->mem_start, virt_to_bus(pAC->IoBase)));


	if (!pAC->IoBase){
		retval = 3;
		goto out_release;
	}

	return 0;

 out_release:
//	pci_release_regions(pdev);
// out_disable:
	pci_disable_device(pdev);
	return retval;
}

#ifdef Y2_RECOVERY
/*****************************************************************************
 *
 * 	SkGeHandleKernelTimer - Handle the kernel timer requests
 *
 * Description:
 *	If the requested time interval for the timer has elapsed, 
 *	this function checks the link state.
 *
 * Returns:	N/A
 *
 */
//static void SkGeHandleKernelTimer(unsigned long ptr)  /* holds the pointer to adapter control context */
static void SkGeHandleKernelTimer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
	DEV_NET         *pNet = (DEV_NET*) DeferredContext;
	SkGeCheckTimer(pNet);	
}

/*****************************************************************************
 *
 * 	sk98lin_check_timer - Resume the the card
 *
 * Description:
 *	This function checks the kernel timer
 *
 * Returns: N/A
 *	
 */
void SkGeCheckTimer(
DEV_NET *pNet)  /* holds the pointer to adapter control context */
{
	SK_AC           *pAC = pNet->pAC;
	SK_BOOL		StartTimer = SK_TRUE;
	LARGE_INTEGER deltaTime;

	if (pNet->InRecover)
		return;
	if (pNet->TimerExpired)
		return;
	pNet->TimerExpired = SK_TRUE;

#define TXPORT pAC->TxPort[pNet->PortNr][TX_PRIO_LOW]
#define RXPORT pAC->RxPort[pNet->PortNr]

	if (	(CHIP_ID_YUKON_2(pAC)) &&
		(netif_running(pAC->dev[pNet->PortNr]))) {
		
#ifdef Y2_RX_CHECK
		if (HW_FEATURE(pAC, HWF_WA_DEV_4167)) {
		/* Checks the RX path */
			CheckRxPath(pNet);
		}
#endif

		/* Checkthe transmitter */
		if (!(IS_Q_EMPTY(&TXPORT.TxAQ_working))) {
			if (TXPORT.LastDone != TXPORT.TxALET.Done) {
				TXPORT.LastDone = TXPORT.TxALET.Done;
				pNet->TransmitTimeoutTimer = 0;
			} else {
				pNet->TransmitTimeoutTimer++;
				if (pNet->TransmitTimeoutTimer >= 10) {
					pNet->TransmitTimeoutTimer = 0;
#ifdef CHECK_TRANSMIT_TIMEOUT
					StartTimer =  SK_FALSE;
					SkLocalEventQueue(pAC, SKGE_DRV, 
						SK_DRV_RECOVER,pNet->PortNr,-1,SK_FALSE);
#endif
				}
			} 
		} 

#ifdef CHECK_TRANSMIT_TIMEOUT

#ifndef __INTERRUPT__
//		if (!timer_pending(&pNet->KernelTimer)) {
//			pNet->KernelTimer.expires = jiffies + (HZ/4); /* 100ms */
//			add_timer(&pNet->KernelTimer);
			deltaTime.QuadPart = - (HZ/4);    /* 2.4 sec. */
			KeSetTimer( &pNet->KernelTimer, deltaTime, &pNet->KernelTimerDpc );
			pNet->TimerExpired = SK_FALSE;
//		}
#endif

#endif
	}
}


/*****************************************************************************
*
* CheckRXCounters - Checks the the statistics for RX path hang
*
* Description:
*	This function is called periodical by a timer. 
*
* Notes:
*
* Function Parameters:
*
* Returns:
*	Traffic status
*
*/
static SK_BOOL CheckRXCounters(
DEV_NET *pNet)  /* holds the pointer to adapter control context */
{
	SK_AC           	*pAC = pNet->pAC;
	SK_BOOL bStatus 	= SK_FALSE;

	/* Variable used to store the MAC RX FIFO RP, RPLev*/
	SK_U32			MACFifoRP = 0;
	SK_U32			MACFifoRLev = 0;

	/* Variable used to store the PCI RX FIFO RP, RPLev*/
	SK_U32			RXFifoRP = 0;
	SK_U8			RXFifoRLev = 0;

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("==> CheckRXCounters()\n"));

	/*Check if statistic counters hangs*/
	if (pNet->LastJiffies == pAC->dev[pNet->PortNr]->last_rx) {
		/* Now read the values of read pointer/level from MAC RX FIFO again */
		SK_IN32(pAC->IoBase, MR_ADDR(pNet->PortNr, RX_GMF_RP), &MACFifoRP);
		SK_IN32(pAC->IoBase, MR_ADDR(pNet->PortNr, RX_GMF_RLEV), &MACFifoRLev);

		/* Now read the values of read pointer/level from RX FIFO again */
		SK_IN8(pAC->IoBase, Q_ADDR(pAC->GIni.GP[pNet->PortNr].PRxQOff, Q_RP), &RXFifoRP);
		SK_IN8(pAC->IoBase, Q_ADDR(pAC->GIni.GP[pNet->PortNr].PRxQOff, Q_RL), &RXFifoRLev);

		/* Check if the MAC RX hang */
		if ((MACFifoRP == pNet->PreviousMACFifoRP) &&
			(pNet->PreviousMACFifoRP != 0) &&
			(MACFifoRLev >= pNet->PreviousMACFifoRLev)){
			bStatus = SK_TRUE;
		}

		/* Check if the PCI RX hang */
		if ((RXFifoRP == pNet->PreviousRXFifoRP) &&
			(pNet->PreviousRXFifoRP != 0) &&
			(RXFifoRLev >= pNet->PreviousRXFifoRLev)){
			/*Set the flag to indicate that the RX FIFO hangs*/
			bStatus = SK_TRUE;
		}
	}

	/* Store now the values of counters for next check */
	pNet->LastJiffies = pAC->dev[pNet->PortNr]->last_rx;

	/* Store the values of  read pointer/level from MAC RX FIFO for next test */
	pNet->PreviousMACFifoRP = MACFifoRP;
	pNet->PreviousMACFifoRLev = MACFifoRLev;

	/* Store the values of  read pointer/level from RX FIFO for next test */
	pNet->PreviousRXFifoRP = RXFifoRP;
	pNet->PreviousRXFifoRLev = RXFifoRLev;

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("<== CheckRXCounters()\n"));

	return bStatus;
}

/*****************************************************************************
*
* CheckRxPath - Checks if the RX path
*
* Description:
*	This function is called periodical by a timer. 
*
* Notes:
*
* Function Parameters:
*
* Returns:
*	None.
*
*/
static void  CheckRxPath(
DEV_NET *pNet)  /* holds the pointer to adapter control context */
{
	KIRQL		Irql;    /* for the spin locks    */
	/* Initialize the pAC structure.*/
	SK_AC           	*pAC = pNet->pAC;

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("==> CheckRxPath()\n"));

	/*If the statistics are not changed then could be an RX problem */
	if (CheckRXCounters(pNet)){
		/* 
		 * First we try the simple solution by resetting the Level Timer
		 */

		/* Stop Level Timer of Status BMU */
		SK_OUT8(pAC->IoBase, STAT_LEV_TIMER_CTRL, TIM_STOP);

		/* Start Level Timer of Status BMU */
		SK_OUT8(pAC->IoBase, STAT_LEV_TIMER_CTRL, TIM_START);

		if (!CheckRXCounters(pNet)) {
			return;
		}

		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
		SkLocalEventQueue(pAC, SKGE_DRV,
			SK_DRV_RECOVER,pNet->PortNr,-1,SK_TRUE);

		/* Reset the fifo counters */
		pNet->PreviousMACFifoRP = 0;
		pNet->PreviousMACFifoRLev = 0;
		pNet->PreviousRXFifoRP = 0;
		pNet->PreviousRXFifoRLev = 0;

		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
	}

	SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MSG,
		("<== CheckRxPath()\n"));
}



#endif


#ifdef CONFIG_PM
/*****************************************************************************
 *
 * 	sk98lin_resume - Resume the the card
 *
 * Description:
 *	This function resumes the card into the D0 state
 *
 * Returns: N/A
 *	
 */
static int sk98lin_resume(
struct pci_dev *pdev)   /* the device that is to resume */
{
	struct net_device   *dev  = pci_get_drvdata(pdev);
	DEV_NET		    *pNet = (DEV_NET*) dev->priv;
	SK_AC		    *pAC  = pNet->pAC;
	SK_U16		     PmCtlSts;

	/* Set the power state to D0 */
	pci_set_power_state(pdev, 0);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
	pci_restore_state(pdev);
#else
	pci_restore_state(pdev, pAC->PciState);
#endif

	/* Set the adapter power state to D0 */
	SkPciReadCfgWord(pAC, PCI_PM_CTL_STS, &PmCtlSts);
	PmCtlSts &= ~(PCI_PM_STATE_D3);	/* reset all DState bits */
	PmCtlSts |= PCI_PM_STATE_D0;
	SkPciWriteCfgWord(pAC, PCI_PM_CTL_STS, PmCtlSts);

	/* Reinit the adapter and start the port again */
	pAC->BoardLevel = SK_INIT_DATA;
	SkDrvLeaveDiagMode(pAC);

	netif_device_attach(dev);
	netif_start_queue(dev);
	return 0;
}
 
/*****************************************************************************
 *
 * 	sk98lin_suspend - Suspend the card
 *
 * Description:
 *	This function suspends the card into a defined state
 *
 * Returns: N/A
 *	
 */
static int sk98lin_suspend(
struct pci_dev	*pdev,   /* pointer to the device that is to suspend */
u32		state)  /* what power state is desired by Linux?    */
{
	struct net_device   *dev  = pci_get_drvdata(pdev);
	DEV_NET		    *pNet = (DEV_NET*) dev->priv;
	SK_AC		    *pAC  = pNet->pAC;
	SK_U16		     PciPMControlStatus;
	SK_U16		     PciPMCapabilities;
	SK_MAC_ADDR	     MacAddr;
	int		     i;

	/* GEnesis and first yukon revs do not support power management */
	if (pAC->GIni.GIChipId == CHIP_ID_YUKON) {
		if (pAC->GIni.GIChipRev == 0) {
			return 0; /* power management not supported */
		}
	} 

	if (pAC->GIni.GIChipId == CHIP_ID_GENESIS) {
		return 0; /* not supported for this chipset */
	}

	if (pAC->WolInfo.ConfiguredWolOptions == 0) {
		return 0; /* WOL possible, but disabled via ethtool */
	}

	if(netif_running(dev)) {
		netif_stop_queue(dev); /* stop device if running */
	}

	netif_device_detach(dev);
	
	/* read the PM control/status register from the PCI config space */
	SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CTL_STS), &PciPMControlStatus);

	/* read the power management capabilities from the config space */
	SK_IN16(pAC->IoBase, PCI_C(pAC, PCI_PM_CAP_REG), &PciPMCapabilities);

	/* Enable WakeUp with Magic Packet - get MAC address from adapter */
	for (i = 0; i < SK_MAC_ADDR_LEN; i++) {
		/* virtual address: will be used for data */
		SK_IN8(pAC->IoBase, (B2_MAC_1 + i), &MacAddr.a[i]);
	}

	SkDrvEnterDiagMode(pAC);
	SkEnableWOMagicPacket(pAC, pAC->IoBase, MacAddr);

	pci_enable_wake(pdev, 3, 1);
	pci_enable_wake(pdev, 4, 1);	/* 4 == D3 cold */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
	pci_save_state(pdev);
#else
	pci_save_state(pdev, pAC->PciState);
#endif
	pci_set_power_state(pdev, state); /* set the state */

	return 0;
}


/******************************************************************************
 *
 *	SkEnableWOMagicPacket - Enable Wake on Magic Packet on the adapter
 *
 * Context:
 *	init, pageable
 *	the adapter should be de-initialized before calling this function
 *
 * Returns:
 *	nothing
 */

static void SkEnableWOMagicPacket(
SK_AC         *pAC,      /* Adapter Control Context          */
SK_IOC         IoC,      /* I/O control context              */
SK_MAC_ADDR    MacAddr)  /* MacAddr expected in magic packet */
{
	SK_U16	Word;
	SK_U32	DWord;
	int 	i;
	int	HwPortIndex;
	int	Port = 0;

	/* use Port 0 as long as we do not have any dual port cards which support WOL */
	HwPortIndex = 0;
	DWord = 0;

	SK_OUT16(IoC, 0x0004, 0x0002);	/* clear S/W Reset */
	SK_OUT16(IoC, 0x0f10, 0x0002);	/* clear Link Reset */

	/*
	 * PHY Configuration:
	 * Autonegotioation is enalbed, advertise 10 HD, 10 FD,
	 * 100 HD, and 100 FD.
	 */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_LITE)) {

		SK_OUT16(IoC, 0x0004, 0x0800);			/* enable CLK_RUN */
		SK_OUT8(IoC, 0x0007, 0xa9);			/* enable VAUX */

		/* WA code for COMA mode */
		/* Only for yukon plus based chipsets rev A3 */
		if (pAC->GIni.GIChipRev == CHIP_REV_YU_LITE_A3) {
			SK_IN32(IoC, B2_GP_IO, &DWord);
			DWord |= GP_DIR_9;			/* set to output */
			DWord &= ~GP_IO_9;			/* clear PHY reset (active high) */
			SK_OUT32(IoC, B2_GP_IO, DWord);		/* clear PHY reset */
		}

		if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_LITE) ||
			(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
			SK_OUT32(IoC, 0x0f04, 0x01f04001);	/* set PHY reset */
			SK_OUT32(IoC, 0x0f04, 0x01f04002);	/* clear PHY reset */
		} else {
			SK_OUT8(IoC, 0x0f04, 0x02);		/* clear PHY reset */
		}

		SK_OUT8(IoC, 0x0f00, 0x02);			/* clear MAC reset */
		SkGmPhyWrite(pAC, IoC, Port, 4, 0x01e1);	/* advertise 10/100 HD/FD */
		SkGmPhyWrite(pAC, IoC, Port, 9, 0x0000);	/* do not advertise 1000 HD/FD */
		SkGmPhyWrite(pAC, IoC, Port, 00, 0xB300);	/* 100 MBit, disable Autoneg */
	} else if (pAC->GIni.GIChipId == CHIP_ID_YUKON_FE) {
		SK_OUT8(IoC, 0x0007, 0xa9);			/* enable VAUX */
		SK_OUT8(IoC, 0x0f04, 0x02);			/* clear PHY reset */
		SK_OUT8(IoC, 0x0f00, 0x02);			/* clear MAC reset */
		SkGmPhyWrite(pAC, IoC, Port, 16, 0x0130);	/* Enable Automatic Crossover */
		SkGmPhyWrite(pAC, IoC, Port, 00, 0xB300);	/* 100 MBit, disable Autoneg */
	}


	/*
	 * MAC Configuration:
	 * Set the MAC to 100 HD and enable the auto update features
	 * for Speed, Flow Control and Duplex Mode.
	 * If autonegotiation completes successfully the
	 * MAC takes the link parameters from the PHY.
	 * If the link partner doesn't support autonegotiation
	 * the MAC can receive magic packets if the link partner
	 * uses 100 HD.
	 */
	SK_OUT16(IoC, 0x2804, 0x3832);
   

	/*
	 * Set Up Magic Packet parameters
	 */
	for (i = 0; i < 6; i+=2) {		/* set up magic packet MAC address */
		SK_IN16(IoC, 0x100 + i, &Word);
		SK_OUT16(IoC, 0xf24 + i, Word);
	}

	SK_OUT16(IoC, 0x0f20, 0x0208);		/* enable PME on magic packet */
						/* and on wake up frame */

	/*
	 * Set up PME generation
	 */
	/* set PME legacy mode */
	/* Only for PCI express based chipsets */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_FE)) {
		SkPciReadCfgDWord(pAC, 0x40, &DWord);
		DWord |= 0x8000;
		SkPciWriteCfgDWord(pAC, 0x40, DWord);
	}

	/* clear PME status and switch adapter to DState */
	SkPciReadCfgWord(pAC, 0x4c, &Word);
	Word |= 0x103;
	SkPciWriteCfgWord(pAC, 0x4c, Word);
}	/* SkEnableWOMagicPacket */
#endif


/*****************************************************************************
 *
 * 	FreeResources - release resources allocated for adapter
 *
 * Description:
 *	This function releases the IRQ, unmaps the IO and
 *	frees the desriptor ring.
 *
 * Returns: N/A
 *	
 */
static void FreeResources(struct SK_NET_DEVICE *dev)
{
SK_U32 AllocFlag;
DEV_NET		*pNet;
SK_AC		*pAC;

	if (dev->priv) {
		pNet = (DEV_NET*) dev->priv;
		pAC = pNet->pAC;
		AllocFlag = pAC->AllocFlag;
		if (pAC->PciDev) {
//			pci_release_regions(pAC->PciDev);
		}
		if (AllocFlag & SK_ALLOC_IRQ) {
#ifdef __INTERRUPT__
			free_irq(dev->irq, dev);
#endif

		}
		if (pAC->IoBase) {
//			iounmap(pAC->IoBase);
		}
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2FreeResources(pAC);
		} else {
			BoardFreeMem(pAC);
		}
	}
	
} /* FreeResources */

#if 0
MODULE_AUTHOR("Mirko Lindner <mlindner@syskonnect.de>");
MODULE_DESCRIPTION("SysKonnect SK-NET Gigabit Ethernet SK-98xx driver");
MODULE_LICENSE("GPL");
#endif


#ifdef LINK_SPEED_A
static char *Speed_A[SK_MAX_CARD_PARAM] = LINK_SPEED;
#else
static char *Speed_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef LINK_SPEED_B
static char *Speed_B[SK_MAX_CARD_PARAM] = LINK_SPEED;
#else
static char *Speed_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef AUTO_NEG_A
static char *AutoNeg_A[SK_MAX_CARD_PARAM] = AUTO_NEG_A;
#else
static char *AutoNeg_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef DUP_CAP_A
static char *DupCap_A[SK_MAX_CARD_PARAM] = DUP_CAP_A;
#else
static char *DupCap_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef FLOW_CTRL_A
static char *FlowCtrl_A[SK_MAX_CARD_PARAM] = FLOW_CTRL_A;
#else
static char *FlowCtrl_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef ROLE_A
static char *Role_A[SK_MAX_CARD_PARAM] = ROLE_A;
#else
static char *Role_A[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef AUTO_NEG_B
static char *AutoNeg_B[SK_MAX_CARD_PARAM] = AUTO_NEG_B;
#else
static char *AutoNeg_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef DUP_CAP_B
static char *DupCap_B[SK_MAX_CARD_PARAM] = DUP_CAP_B;
#else
static char *DupCap_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef FLOW_CTRL_B
static char *FlowCtrl_B[SK_MAX_CARD_PARAM] = FLOW_CTRL_B;
#else
static char *FlowCtrl_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef ROLE_B
static char *Role_B[SK_MAX_CARD_PARAM] = ROLE_B;
#else
static char *Role_B[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef CON_TYPE
static char *ConType[SK_MAX_CARD_PARAM] = CON_TYPE;
#else
static char *ConType[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef PREF_PORT
static char *PrefPort[SK_MAX_CARD_PARAM] = PREF_PORT;
#else
static char *PrefPort[SK_MAX_CARD_PARAM] = {"", };
#endif

#ifdef RLMT_MODE
static char *RlmtMode[SK_MAX_CARD_PARAM] = RLMT_MODE;
#else
static char *RlmtMode[SK_MAX_CARD_PARAM] = {"", };
#endif

static int   IntsPerSec[SK_MAX_CARD_PARAM];
static char *Moderation[SK_MAX_CARD_PARAM];
static char *ModerationMask[SK_MAX_CARD_PARAM];

static char *LowLatency[SK_MAX_CARD_PARAM];

#if 0
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(Speed_A, charp, NULL, 0);
module_param_array(Speed_B, charp, NULL, 0);
module_param_array(AutoNeg_A, charp, NULL, 0);
module_param_array(AutoNeg_B, charp, NULL, 0);
module_param_array(DupCap_A, charp, NULL, 0);
module_param_array(DupCap_B, charp, NULL, 0);
module_param_array(FlowCtrl_A, charp, NULL, 0);
module_param_array(FlowCtrl_B, charp, NULL, 0);
module_param_array(Role_A, charp, NULL, 0);
module_param_array(Role_B, charp, NULL, 0);
module_param_array(ConType, charp, NULL, 0);
module_param_array(PrefPort, charp, NULL, 0);
module_param_array(RlmtMode, charp, NULL, 0);
/* used for interrupt moderation */
module_param_array(IntsPerSec, int, NULL, 0);
module_param_array(Moderation, charp, NULL, 0);
module_param_array(ModerationMask, charp, NULL, 0);
module_param_array(LowLatency, charp, NULL, 0);
#else
MODULE_PARM(Speed_A,    "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(Speed_B,    "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(AutoNeg_A,  "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(AutoNeg_B,  "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(DupCap_A,   "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(DupCap_B,   "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(FlowCtrl_A, "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(FlowCtrl_B, "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(Role_A,	"1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(Role_B,	"1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(ConType,	"1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(PrefPort,   "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(RlmtMode,   "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(IntsPerSec,     "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "i");
MODULE_PARM(Moderation,     "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(ModerationMask, "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
MODULE_PARM(LowLatency, "1-" __MODULE_STRING(SK_MAX_CARD_PARAM) "s");
#endif
#endif

/*****************************************************************************
 *
 * 	sk98lin_remove_device - device deinit function
 *
 * Description:
 *	Disable adapter if it is still running, free resources,
 *	free device struct.
 *
 * Returns: N/A
 */

static void sk98lin_remove_device(struct pci_dev *pdev)
{
DEV_NET		*pNet;
SK_AC		*pAC;
struct SK_NET_DEVICE *next;
KIRQL Irql;
struct device *dev = pci_get_drvdata(pdev);


	/* Device not available. Return. */
	if (!dev)
		return;

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	next = pAC->Next;

	netif_stop_queue(dev);
	SkGeYellowLED(pAC, pAC->IoBase, 0);

	if(pAC->BoardLevel == SK_INIT_RUN) {
		/* board is still alive */
		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					0, -1, SK_FALSE);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					1, -1, SK_TRUE);

		/* disable interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		SkGeDeInit(pAC, pAC->IoBase);
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
		pAC->BoardLevel = SK_INIT_DATA;
		/* We do NOT check here, if IRQ was pending, of course*/
	}

	if(pAC->BoardLevel == SK_INIT_IO) {
		/* board is still alive */
		SkGeDeInit(pAC, pAC->IoBase);
		pAC->BoardLevel = SK_INIT_DATA;
	}

	if ((pAC->GIni.GIMacsFound == 2) && pAC->RlmtNets == 2){
		kfree(pAC->dev[1]);
	}

	FreeResources(dev);

#ifdef CONFIG_PROC_FS
	/* Remove the sk98lin procfs device entries */
	if ((pAC->GIni.GIMacsFound == 2) && pAC->RlmtNets == 2){
		remove_proc_entry(pAC->dev[1]->name, pSkRootDir);
	}
	remove_proc_entry(pNet->InitialDevName, pSkRootDir);
#endif

	dev->get_stats = NULL;
	/*
	 * otherwise unregister_netdev calls get_stats with
	 * invalid IO ...  :-(
	 */
//	kfree(dev);
	kfree(pAC);
	sk98lin_max_boards_found--;

#ifdef CONFIG_PROC_FS
	/* Remove all Proc entries if last device */
	if (sk98lin_max_boards_found == 0) {
		/* clear proc-dir */
		remove_proc_entry(pSkRootDir->name, proc_net);
	}
#endif

}


/*****************************************************************************
 *
 * 	SkGeBoardInit - do level 0 and 1 initialization
 *
 * Description:
 *	This function prepares the board hardware for running. The desriptor
 *	ring is set up, the IRQ is allocated and the configuration settings
 *	are examined.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int __init SkGeBoardInit(struct SK_NET_DEVICE *dev, SK_AC *pAC)
{
short	i;
KIRQL Irql;
char	*DescrString = "sk98lin: Driver for Linux"; /* this is given to PNMI */
char	*VerStr	= VER_STRING;
int	Ret;			/* return code of request_irq */
SK_BOOL	DualNet;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeBoardInit Entered, IoBase: %08lX\n", (unsigned long)pAC->IoBase));
	for (i=0; i<SK_MAX_MACS; i++) {
		pAC->TxPort[i][0].HwAddr = pAC->IoBase + TxQueueAddr[i][0];
		pAC->TxPort[i][0].PortIndex = i;
		pAC->RxPort[i].HwAddr = pAC->IoBase + RxQueueAddr[i];
		pAC->RxPort[i].PortIndex = i;
	}

	/* Initialize the mutexes */
	for (i=0; i<SK_MAX_MACS; i++) {
		KeInitializeSpinLock(&pAC->TxPort[i][0].TxDesRingLock);
		KeInitializeSpinLock(&pAC->RxPort[i].RxDesRingLock);
	}

	KeInitializeSpinLock(&pAC->SlowPathLock);
	KeInitializeSpinLock(&pAC->TxQueueLock);	/* for Yukon2 chipsets */
	KeInitializeSpinLock(&pAC->SetPutIndexLock);	/* for Yukon2 chipsets */

	/* level 0 init common modules here */
	
	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
	/* Does a RESET on board ...*/
	if (SkGeInit(pAC, pAC->IoBase, SK_INIT_DATA) != 0) {
		NbDebugPrint(0, ("HWInit (0) failed.\n"));
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
		return(-EAGAIN);
	}
	SkI2cInit(  pAC, pAC->IoBase, SK_INIT_DATA);
	SkEventInit(pAC, pAC->IoBase, SK_INIT_DATA);
	SkPnmiInit( pAC, pAC->IoBase, SK_INIT_DATA);
	SkAddrInit( pAC, pAC->IoBase, SK_INIT_DATA);
	SkRlmtInit( pAC, pAC->IoBase, SK_INIT_DATA);
	SkTimerInit(pAC, pAC->IoBase, SK_INIT_DATA);

	pAC->BoardLevel = SK_INIT_DATA;
	pAC->RxPort[0].RxBufSize = ETH_BUF_SIZE;
	pAC->RxPort[1].RxBufSize = ETH_BUF_SIZE;

	SK_PNMI_SET_DRIVER_DESCR(pAC, DescrString);
	SK_PNMI_SET_DRIVER_VER(pAC, VerStr);

	/* level 1 init common modules here (HW init) */
	if (SkGeInit(pAC, pAC->IoBase, SK_INIT_IO) != 0) {
		NbDebugPrint(0, ("sk98lin: HWInit (1) failed.\n");
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql));
		return(-EAGAIN);
	}
	SkI2cInit(  pAC, pAC->IoBase, SK_INIT_IO);
	SkEventInit(pAC, pAC->IoBase, SK_INIT_IO);
	SkPnmiInit( pAC, pAC->IoBase, SK_INIT_IO);
	SkAddrInit( pAC, pAC->IoBase, SK_INIT_IO);
	SkRlmtInit( pAC, pAC->IoBase, SK_INIT_IO);
	SkTimerInit(pAC, pAC->IoBase, SK_INIT_IO);
#ifdef Y2_RECOVERY
	/* mark entries invalid */
	pAC->LastPort = 3;
	pAC->LastOpc = 0xFF;
#endif

	/* Set chipset type support */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_LITE) ||
		(pAC->GIni.GIChipId == CHIP_ID_YUKON_LP)) {
		pAC->ChipsetType = 1;	/* Yukon chipset (descriptor logic) */
	} else if (CHIP_ID_YUKON_2(pAC)) {
		pAC->ChipsetType = 2;	/* Yukon2 chipset (list logic) */
	} else {
		pAC->ChipsetType = 0;	/* Genesis chipset (descriptor logic) */
	}

	/* wake on lan support */
	pAC->WolInfo.SupportedWolOptions = 0;
#if defined (ETHTOOL_GWOL) && defined (ETHTOOL_SWOL)
	if (pAC->GIni.GIChipId != CHIP_ID_GENESIS) {
		pAC->WolInfo.SupportedWolOptions  = WAKE_MAGIC;
		if (pAC->GIni.GIChipId == CHIP_ID_YUKON) {
			if (pAC->GIni.GIChipRev == 0) {
				pAC->WolInfo.SupportedWolOptions = 0;
			}
		} 
	}
#endif
	pAC->WolInfo.ConfiguredWolOptions = pAC->WolInfo.SupportedWolOptions;

	GetConfiguration(pAC);
	if (pAC->RlmtNets == 2) {
		pAC->GIni.GP[0].PPortUsage = SK_MUL_LINK;
		pAC->GIni.GP[1].PPortUsage = SK_MUL_LINK;
	}

	pAC->BoardLevel = SK_INIT_IO;
	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);

#ifdef __INTERRUPT__
	/////////////////////////////////////////////////////////////////////////
	nic_irq = dev->irq;
	/////////////////////////////////////////////////////////////////////////

	NbDebugPrint(0, ("nic_irq = %d\n", nic_irq));

	if (!CHIP_ID_YUKON_2(pAC)) {
		if (pAC->GIni.GIMacsFound == 2) {
			Ret = request_irq(dev->irq, SkGeIsr, SA_SHIRQ, dev->name, dev);
		} else if (pAC->GIni.GIMacsFound == 1) {
			Ret = request_irq(dev->irq, SkGeIsrOnePort, SA_SHIRQ, dev->name, dev);
		} else {
			NbDebugPrint(0, ("sk98lin: Illegal number of ports: %d\n",
				pAC->GIni.GIMacsFound));
			return -EAGAIN;
		}
	}
	else {
		Ret = request_irq(dev->irq, SkY2Isr, SA_SHIRQ, dev->name, dev);
	}

	if (Ret) {
		NbDebugPrint(0, ("sk98lin: Requested IRQ %d is busy.\n",
			dev->irq));
		return -EAGAIN;
	}
#else

#ifndef __ENABLE_LOADER__
	for(i=0;i<HIGH_LEVEL;i++) {
		HalDisableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		(KIRQL)i
		);
	}

/*
		HalDisableSystemInterrupt (
		PRIMARY_VECTOR_BASE+dev->irq,
		(KIRQL)PASSIVE_LEVEL
		);
*/
#endif
#endif

	pAC->AllocFlag |= SK_ALLOC_IRQ;

	/* 
	** Alloc descriptor/LETable memory for this board (both RxD/TxD)
	*/
	if (CHIP_ID_YUKON_2(pAC)) {
		if (!SkY2AllocateResources(pAC)) {
			NbDebugPrint(0, ("No memory for Yukon2 settings\n"));
			return(-EAGAIN);
		}
	} else {
		if(!BoardAllocMem(pAC)) {
			NbDebugPrint(0, ("No memory for descriptor rings.\n"));
			return(-EAGAIN);
		}
	}

#ifdef SK_USE_CSUM
	SkCsSetReceiveFlags(pAC,
		SKCS_PROTO_IP | SKCS_PROTO_TCP | SKCS_PROTO_UDP,
		&pAC->CsOfs1, &pAC->CsOfs2, 0);
	pAC->CsOfs = (pAC->CsOfs2 << 16) | pAC->CsOfs1;
#endif

	/*
	** Function BoardInitMem() for Yukon dependent settings...
	*/
	BoardInitMem(pAC);
	/* tschilling: New common function with minimum size check. */
	DualNet = SK_FALSE;
	if (pAC->RlmtNets == 2) {
		DualNet = SK_TRUE;
	}
	
	if (SkGeInitAssignRamToQueues(
		pAC,
		pAC->ActivePort,
		DualNet)) {
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2FreeResources(pAC);
		} else {
			BoardFreeMem(pAC);
		}

		NbDebugPrint(0, ("sk98lin: SkGeInitAssignRamToQueues failed.\n"));
		return(-EAGAIN);
	}

	/*
	 * Register the device here
	 */
	pAC->Next = SkGeRootDev;
	SkGeRootDev = dev;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeBoardInit Exit\n"));

	return (0);
} /* SkGeBoardInit */


/*****************************************************************************
 *
 * 	BoardAllocMem - allocate the memory for the descriptor rings
 *
 * Description:
 *	This function allocates the memory for all descriptor rings.
 *	Each ring is aligned for the desriptor alignment and no ring
 *	has a 4 GByte boundary in it (because the upper 32 bit must
 *	be constant for all descriptiors in one rings).
 *
 * Returns:
 *	SK_TRUE, if all memory could be allocated
 *	SK_FALSE, if not
 */
static SK_BOOL BoardAllocMem(
SK_AC	*pAC)
{
caddr_t		pDescrMem;	/* pointer to descriptor memory area */
size_t		AllocLength;	/* length of complete descriptor area */
int		i;		/* loop counter */
unsigned long	BusAddr;

	NbDebugPrint(3, ("BoardAllocMem Entered"));
	
	/* rings plus one for alignment (do not cross 4 GB boundary) */
	/* RX_RING_SIZE is assumed bigger than TX_RING_SIZE */
#if (BITS_PER_LONG == 32)
	AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound + 8;
#else
	AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound
		+ RX_RING_SIZE + 8;
#endif

	pDescrMem = pci_alloc_consistent(pAC->PciDev, AllocLength,
					 &pAC->pDescrMemDMA);

	if (pDescrMem == NULL) {
		return (SK_FALSE);
	}
	pAC->pDescrMem = pDescrMem;
	BusAddr = (unsigned long) pAC->pDescrMemDMA;

	/* Descriptors need 8 byte alignment, and this is ensured
	 * by pci_alloc_consistent.
	 */
	for (i=0; i<pAC->GIni.GIMacsFound; i++) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS,
			("BoardAllocMem:TX%d/A: pDescrMem: %lX,   PhysDescrMem: %lX\n",
			i, (unsigned long) pDescrMem,
			BusAddr));

		NbDebugPrint(0, ("BoardAllocMem:TX%d/A: pDescrMem: %lX,   PhysDescrMem: %lX\n",
			i, (unsigned long) pDescrMem,
			BusAddr));

		pAC->TxPort[i][0].pTxDescrRing = pDescrMem;
		pAC->TxPort[i][0].VTxDescrRing = BusAddr;
		pDescrMem += TX_RING_SIZE;
		BusAddr += TX_RING_SIZE;
	
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS,
			("BoardAllocMem:RX%d: pDescrMem: %lX,   PhysDescrMem: %lX\n",
			i, (unsigned long) pDescrMem,
			(unsigned long)BusAddr));

		NbDebugPrint(0, ("BoardAllocMem:RX%d: pDescrMem: %lX,   PhysDescrMem: %lX\n",
			i, (unsigned long) pDescrMem,
			(unsigned long)BusAddr));

		pAC->RxPort[i].pRxDescrRing = pDescrMem;
		pAC->RxPort[i].VRxDescrRing = BusAddr;
		pDescrMem += RX_RING_SIZE;
		BusAddr += RX_RING_SIZE;
	} /* for */
	
	return (SK_TRUE);
} /* BoardAllocMem */


/****************************************************************************
 *
 *	BoardFreeMem - reverse of BoardAllocMem
 *
 * Description:
 *	Free all memory allocated in BoardAllocMem: adapter context,
 *	descriptor rings, locks.
 *
 * Returns:	N/A
 */
static void BoardFreeMem(
SK_AC		*pAC)
{
size_t		AllocLength;	/* length of complete descriptor area */

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("BoardFreeMem\n"));

	if (pAC->pDescrMem) {

#if (BITS_PER_LONG == 32)
		AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound + 8;
#else
		AllocLength = (RX_RING_SIZE + TX_RING_SIZE) * pAC->GIni.GIMacsFound
			+ RX_RING_SIZE + 8;
#endif

		pci_free_consistent(pAC->PciDev, AllocLength,
			    pAC->pDescrMem, pAC->pDescrMemDMA);
		pAC->pDescrMem = NULL;
	}
} /* BoardFreeMem */


/*****************************************************************************
 *
 * 	BoardInitMem - initiate the descriptor rings
 *
 * Description:
 *	This function sets the descriptor rings or LETables up in memory.
 *	The adapter is initialized with the descriptor start addresses.
 *
 * Returns:	N/A
 */
static void BoardInitMem(
SK_AC	*pAC)	/* pointer to adapter context */
{
int	i;		/* loop counter */
int	RxDescrSize;	/* the size of a rx descriptor rounded up to alignment*/
int	TxDescrSize;	/* the size of a tx descriptor rounded up to alignment*/

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("BoardInitMem Entered\n"));

	if (!pAC->GIni.GIYukon2) {
		RxDescrSize = (((sizeof(RXD) - 1) / DESCR_ALIGN) + 1) * DESCR_ALIGN;
		pAC->RxDescrPerRing = RX_RING_SIZE / RxDescrSize;
		TxDescrSize = (((sizeof(TXD) - 1) / DESCR_ALIGN) + 1) * DESCR_ALIGN;
		pAC->TxDescrPerRing = TX_RING_SIZE / RxDescrSize;
	
		for (i=0; i<pAC->GIni.GIMacsFound; i++) {
			SetupRing(
				pAC,
				pAC->TxPort[i][0].pTxDescrRing,
				(ULONG)pAC->TxPort[i][0].VTxDescrRing, // conversion correct ?
				(RXD**)&pAC->TxPort[i][0].pTxdRingHead,
				(RXD**)&pAC->TxPort[i][0].pTxdRingTail,
				(RXD**)&pAC->TxPort[i][0].pTxdRingPrev,
				&pAC->TxPort[i][0].TxdRingFree,
				&pAC->TxPort[i][0].TxdRingPrevFree,
				SK_TRUE);
			SetupRing(
				pAC,
				pAC->RxPort[i].pRxDescrRing,
				(ULONG)pAC->RxPort[i].VRxDescrRing, // conversion correct ?
				&pAC->RxPort[i].pRxdRingHead,
				&pAC->RxPort[i].pRxdRingTail,
				&pAC->RxPort[i].pRxdRingPrev,
				&pAC->RxPort[i].RxdRingFree,
				&pAC->RxPort[i].RxdRingFree,
				SK_FALSE);
		}
	}

} /* BoardInitMem */

/*****************************************************************************
 *
 * 	SetupRing - create one descriptor ring
 *
 * Description:
 *	This function creates one descriptor ring in the given memory area.
 *	The head, tail and number of free descriptors in the ring are set.
 *
 * Returns:
 *	none
 */
static void SetupRing(
SK_AC		*pAC,
void		*pMemArea,	/* a pointer to the memory area for the ring */
uintptr_t2	VMemArea,	/* the virtual bus address of the memory area */
RXD		**ppRingHead,	/* address where the head should be written */
RXD		**ppRingTail,	/* address where the tail should be written */
RXD		**ppRingPrev,	/* address where the tail should be written */
int		*pRingFree,	/* address where the # of free descr. goes */
int		*pRingPrevFree,	/* address where the # of free descr. goes */
SK_BOOL		IsTx)		/* flag: is this a tx ring */
{
int	i;		/* loop counter */
int	DescrSize;	/* the size of a descriptor rounded up to alignment*/
int	DescrNum;	/* number of descriptors per ring */
RXD	*pDescr;	/* pointer to a descriptor (receive or transmit) */
RXD	*pNextDescr;	/* pointer to the next descriptor */
RXD	*pPrevDescr;	/* pointer to the previous descriptor */
uintptr_t2 VNextDescr;	/* the virtual bus address of the next descriptor */

	if (IsTx == SK_TRUE) {
		DescrSize = (((sizeof(TXD) - 1) / DESCR_ALIGN) + 1) *
			DESCR_ALIGN;
		DescrNum = TX_RING_SIZE / DescrSize;
	} else {
		DescrSize = (((sizeof(RXD) - 1) / DESCR_ALIGN) + 1) *
			DESCR_ALIGN;
		DescrNum = RX_RING_SIZE / DescrSize;
	}
	
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS,
		("Descriptor size: %d   Descriptor Number: %d\n",
		DescrSize,DescrNum));
	
	pDescr = (RXD*) pMemArea;
	
	pPrevDescr = NULL;
	pNextDescr = (RXD*) (((char*)pDescr) + DescrSize);
	VNextDescr = VMemArea + DescrSize;
	for(i=0; i<DescrNum; i++) {
		memset((void *)pDescr, 0,  sizeof(RXD));

		/* set the pointers right */
		pDescr->VNextRxd = VNextDescr & 0xffffffffULL;
		pDescr->pNextRxd = pNextDescr;
		pDescr->TcpSumStarts = pAC->CsOfs;

		/* advance one step */
		pPrevDescr = pDescr;
		pDescr = pNextDescr;
		pNextDescr = (RXD*) (((char*)pDescr) + DescrSize);
		VNextDescr += DescrSize;
	}
	pPrevDescr->pNextRxd = (RXD*) pMemArea;
	pPrevDescr->VNextRxd = VMemArea;	
	pDescr               = (RXD*) pMemArea;
	*ppRingHead          = (RXD*) pMemArea;
	*ppRingTail          = *ppRingHead;
	*ppRingPrev          = pPrevDescr;
	*pRingFree           = DescrNum;
	*pRingPrevFree       = DescrNum;
} /* SetupRing */


/*****************************************************************************
 *
 * 	PortReInitBmu - re-initiate the descriptor rings for one port
 *
 * Description:
 *	This function reinitializes the descriptor rings of one port
 *	in memory. The port must be stopped before.
 *	The HW is initialized with the descriptor start addresses.
 *
 * Returns:
 *	none
 */
static void PortReInitBmu(
SK_AC	*pAC,		/* pointer to adapter context */
int	PortIndex)	/* index of the port for which to re-init */
{
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("PortReInitBmu "));

	/* set address of first descriptor of ring in BMU */
	{
		ULONG Val32;

		SK_IN32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L,&Val32);
		NbDebugPrint(3, ("TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L = %08X\n", Val32));
		SK_IN32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H,&Val32);
		NbDebugPrint(3, ("TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H = %08X\n", Val32));
		SK_IN32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_L,	&Val32);
		NbDebugPrint(3, ("RxQueueAddr[PortIndex]+Q_DA_L = %08X\n", Val32));
		SK_IN32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_H, &Val32);
		NbDebugPrint(3, ("RxQueueAddr[PortIndex]+Q_DA_H = %08X\n", Val32));
	}

	SK_OUT32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L,
		(uint32_t)(((caddr_t)
		(pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxdRingHead) -
		pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxDescrRing +
		pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing) &
		0xFFFFFFFF));
	SK_OUT32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H,
		(uint32_t)(((caddr_t)
		(pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxdRingHead) -
		pAC->TxPort[PortIndex][TX_PRIO_LOW].pTxDescrRing +
		pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing) >> 32));
	SK_OUT32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_L,
		(uint32_t)(((caddr_t)(pAC->RxPort[PortIndex].pRxdRingHead) -
		pAC->RxPort[PortIndex].pRxDescrRing +
		pAC->RxPort[PortIndex].VRxDescrRing) & 0xFFFFFFFF));
	SK_OUT32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_H,
		(uint32_t)(((caddr_t)(pAC->RxPort[PortIndex].pRxdRingHead) -
		pAC->RxPort[PortIndex].pRxDescrRing +
		pAC->RxPort[PortIndex].VRxDescrRing) >> 32));

	{
		ULONG Val32;

		SK_IN32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L,&Val32);
		NbDebugPrint(3, ("TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L = %08X\n", Val32));
		SK_IN32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H,&Val32);
		NbDebugPrint(3, ("TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H = %08X\n", Val32));
		SK_IN32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_L,	&Val32);
		NbDebugPrint(3, ("RxQueueAddr[PortIndex]+Q_DA_L = %08X\n", Val32));
		SK_IN32(pAC->IoBase, RxQueueAddr[PortIndex]+Q_DA_H, &Val32);
		NbDebugPrint(3, ("RxQueueAddr[PortIndex]+Q_DA_H = %08X\n", Val32));
	}
} /* PortReInitBmu */

/****************************************************************************
 *
 *	SkGePollTimer - handle adapter interrupts
 *
 * Description:
 *	The interrupt routine is called when the network adapter
 *	generates an interrupt. It may also be called if another device
 *	shares this interrupt vector with the driver.
 *
 * Returns: N/A
 *
 */

static void SkGePollTimer(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
struct SK_NET_DEVICE *dev = (struct device *)DeferredContext;
DEV_NET		*pNet;
SK_AC		*pAC;
SK_U32		IntSrc;		/* interrupts source register contents */	
LARGE_INTEGER deltaTime;
int		WorkToDo = 256;
int		WorkDone = 0;
KIRQL	Irql;       

	NbDebugPrint(0, ("SkGePollTimer Enetered\n"));

#ifndef __INTERRUPT__
	KeCancelTimer(&SkGeInterruptTimer);
#endif

	if(STATUS_NIC_OK != SkGe_get_status(dev)) {		
		return;
	}

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;	

	if (pAC->dev[0] != pAC->dev[1]) {
#ifdef USE_TX_COMPLETE
		KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
		FreeTxDescriptors(pAC, &pAC->TxPort[1][TX_PRIO_LOW]);
		KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
#endif
		ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE);
		CLEAR_AND_START_RX(1);
	}
#ifdef USE_TX_COMPLETE
	KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
	FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
	KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
#endif
	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
	CLEAR_AND_START_RX(0);

	dev_xmit_all(NULL);

#ifndef __INTERRUPT__	
	deltaTime.QuadPart = - INTERRUPT_TIME;    
	(VOID) KeSetTimer( &SkGeInterruptTimer, deltaTime, &SkGeInterruptTimerDpc );
#endif	

	NbDebugPrint(3, ("SkGePollTimer Exit\n"));

	return;
} /* SkGeIsrOnePort */


/****************************************************************************
 *
 *	SkGeIsr - handle adapter interrupts
 *
 * Description:
 *	The interrupt routine is called when the network adapter
 *	generates an interrupt. It may also be called if another device
 *	shares this interrupt vector with the driver.
 *
 * Returns: N/A
 *
 */
//static SkIsrRetVar SkGeIsr(int irq, void *dev_id, struct pt_regs *ptregs)
static void SkGeIsr(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
struct SK_NET_DEVICE *dev = (struct device *)DeferredContext;
DEV_NET		*pNet;
SK_AC		*pAC;
SK_U32		IntSrc;		/* interrupts source register contents */	
LARGE_INTEGER deltaTime;

	NbDebugPrint(3, ("SkGeIsr Entered !!\n"));

#ifndef __INTERRUPT__
	KeCancelTimer(&SkGeInterruptTimer);
#endif

	if(STATUS_NIC_OK != SkGe_get_status(dev)) {		
		return;
	}

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	
	/*
	 * Check and process if its our interrupt
	 */
	SK_IN32(pAC->IoBase, B0_SP_ISRC, &IntSrc);
	if ((IntSrc == 0) && (!pNet->NetConsoleMode)) {
#ifndef __INTERRUPT__
		deltaTime.QuadPart = - INTERRUPT_TIME;    
		(VOID) KeSetTimer( &SkGeInterruptTimer, deltaTime, &SkGeInterruptTimerDpc );
#endif
		return;
	}

#ifdef CONFIG_SK98LIN_NAPI
	if (netif_rx_schedule_prep(dev)) {
		pAC->GIni.GIValIrqMask &= ~(NAPI_DRV_IRQS);
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		__netif_rx_schedule(dev);
	}

#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
	if (IntSrc & IS_XA1_F) {
		CLEAR_TX_IRQ(0, TX_PRIO_LOW);
	}
	if (IntSrc & IS_XA2_F) {
		CLEAR_TX_IRQ(1, TX_PRIO_LOW);
	}
#endif


#else
	while (((IntSrc & IRQ_MASK) & ~SPECIAL_IRQS) != 0) {
#if 0 /* software irq currently not used */
		if (IntSrc & IS_IRQ_SW) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("Software IRQ\n"));
		}
#endif
		if (IntSrc & IS_R1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF RX1 IRQ\n"));
			ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
			CLEAR_AND_START_RX(0);
			SK_PNMI_CNT_RX_INTR(pAC, 0);
		}
		if (IntSrc & IS_R2_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF RX2 IRQ\n"));
			ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE);
			CLEAR_AND_START_RX(1);
			SK_PNMI_CNT_RX_INTR(pAC, 1);
		}
#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
		if (IntSrc & IS_XA1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF AS TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_LOW);
			SK_PNMI_CNT_TX_INTR(pAC, 0);
			KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
			FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
		}
		if (IntSrc & IS_XA2_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF AS TX2 IRQ\n"));
			CLEAR_TX_IRQ(1, TX_PRIO_LOW);
			SK_PNMI_CNT_TX_INTR(pAC, 1);
			KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
			FreeTxDescriptors(pAC, &pAC->TxPort[1][TX_PRIO_LOW]);
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
		}
#if 0 /* only if sync. queues used */
		if (IntSrc & IS_XS1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF SY TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_HIGH);
			SK_PNMI_CNT_TX_INTR(pAC, 1);
			KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
			FreeTxDescriptors(pAC, 0, TX_PRIO_HIGH);
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
		}
		if (IntSrc & IS_XS2_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF SY TX2 IRQ\n"));
			CLEAR_TX_IRQ(1, TX_PRIO_HIGH);
			SK_PNMI_CNT_TX_INTR(pAC, 1);
			KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[1][TX_PRIO_HIGH].TxDesRingLock);
			FreeTxDescriptors(pAC, 1, TX_PRIO_HIGH);
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[1][TX_PRIO_HIGH].TxDesRingLock);
		}
#endif
#endif

		SK_IN32(pAC->IoBase, B0_ISRC, &IntSrc);
	} /* while (IntSrc & IRQ_MASK != 0) */
#endif

	IntSrc &= pAC->GIni.GIValIrqMask;
	if ((IntSrc & SPECIAL_IRQS) || pAC->CheckQueue) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_INT_SRC,
			("SPECIAL IRQ DP-Cards => %x\n", IntSrc));
		pAC->CheckQueue = SK_FALSE;
		KeAcquireSpinLockAtDpcLevel(&pAC->SlowPathLock);
		if (IntSrc & SPECIAL_IRQS)
			SkGeSirqIsr(pAC, pAC->IoBase, IntSrc);

		SkEventDispatcher(pAC, pAC->IoBase);
		KeReleaseSpinLockFromDpcLevel(&pAC->SlowPathLock);
	}

#ifndef CONFIG_SK98LIN_NAPI
	/* Handle interrupts */
	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
	ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE);
#endif

	if (pAC->CheckQueue) {
		pAC->CheckQueue = SK_FALSE;
		KeAcquireSpinLockAtDpcLevel(&pAC->SlowPathLock);
		SkEventDispatcher(pAC, pAC->IoBase);
		KeReleaseSpinLockFromDpcLevel(&pAC->SlowPathLock);
	}

	/* IRQ is processed - Enable IRQs again*/
	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);

	dev_xmit_all(NULL);

#ifndef __INTERRUPT__
	deltaTime.QuadPart = - INTERRUPT_TIME;    
	(VOID) KeSetTimer( &SkGeInterruptTimer, deltaTime, &SkGeInterruptTimerDpc );
#endif

	return;
} /* SkGeIsr */


/****************************************************************************
 *
 *	SkGeIsrOnePort - handle adapter interrupts for single port adapter
 *
 * Description:
 *	The interrupt routine is called when the network adapter
 *	generates an interrupt. It may also be called if another device
 *	shares this interrupt vector with the driver.
 *	This is the same as above, but handles only one port.
 *
 * Returns: N/A
 *
 */
//static SkIsrRetVar SkGeIsrOnePort(int irq, void *dev_id, struct pt_regs *ptregs)
static void SkGeIsrOnePort(
	IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
)
{
struct SK_NET_DEVICE *dev = (struct device *)DeferredContext;
DEV_NET		*pNet;
SK_AC		*pAC;
SK_U32		IntSrc;		/* interrupts source register contents */	
LARGE_INTEGER deltaTime;

	NbDebugPrint(3, ("SkGeIsrOnePort Enetered\n"));

#ifndef __INTERRUPT__
	KeCancelTimer(&SkGeInterruptTimer);
#endif

	if(STATUS_NIC_OK != SkGe_get_status(dev)) {
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

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	
#if xDEBUG
	{
		SK_U32	IntSrc2;

		SK_IN32(pAC->IoBase, B0_ISRC, &IntSrc2);
		NbDebugPrint(0, ("IntSrc2 = %08X\n", IntSrc2));
	}
#endif
	/*
	 * Check and process if its our interrupt
	 */
	SK_IN32(pAC->IoBase, B0_SP_ISRC, &IntSrc);
	if ((IntSrc == 0) && (!pNet->NetConsoleMode)) {
#ifndef __INTERRUPT__
		deltaTime.QuadPart = - INTERRUPT_TIME;    
		(VOID) KeSetTimer( &SkGeInterruptTimer, deltaTime, &SkGeInterruptTimerDpc );
#endif
		return;
	}
	
#ifdef CONFIG_SK98LIN_NAPI
	if (netif_rx_schedule_prep(dev)) {
		CLEAR_AND_START_RX(0);
		CLEAR_TX_IRQ(0, TX_PRIO_LOW);
		pAC->GIni.GIValIrqMask &= ~(NAPI_DRV_IRQS);
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		__netif_rx_schedule(dev);
	} 

#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
	if (IntSrc & IS_XA1_F) {
		CLEAR_TX_IRQ(0, TX_PRIO_LOW);
	}
#endif
#else
	while (((IntSrc & IRQ_MASK) & ~SPECIAL_IRQS) != 0) {
#if 0 /* software irq currently not used */
		if (IntSrc & IS_IRQ_SW) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("Software IRQ\n"));
		}
#endif
		if (IntSrc & IS_R1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF RX1 IRQ\n"));
			ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
			CLEAR_AND_START_RX(0);
			SK_PNMI_CNT_RX_INTR(pAC, 0);
		}
#ifdef USE_TX_COMPLETE /* only if tx complete interrupt used */
		if (IntSrc & IS_XA1_F) {			
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF AS TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_LOW);
			SK_PNMI_CNT_TX_INTR(pAC, 0);
			KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
			FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
		}
#if 0 /* only if sync. queues used */
		if (IntSrc & IS_XS1_F) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_INT_SRC,
				("EOF SY TX1 IRQ\n"));
			CLEAR_TX_IRQ(0, TX_PRIO_HIGH);
			SK_PNMI_CNT_TX_INTR(pAC, 0);
			KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
			FreeTxDescriptors(pAC, 0, TX_PRIO_HIGH);
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[0][TX_PRIO_HIGH].TxDesRingLock);
		}
#endif
#endif

		SK_IN32(pAC->IoBase, B0_ISRC, &IntSrc);
	} /* while (IntSrc & IRQ_MASK != 0) */
#endif
	
	IntSrc &= pAC->GIni.GIValIrqMask;
	if ((IntSrc & SPECIAL_IRQS) || pAC->CheckQueue) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_INT_SRC,
			("SPECIAL IRQ SP-Cards => %x\n", IntSrc));
		pAC->CheckQueue = SK_FALSE;
		KeAcquireSpinLockAtDpcLevel(&pAC->SlowPathLock);
		if (IntSrc & SPECIAL_IRQS)
			SkGeSirqIsr(pAC, pAC->IoBase, IntSrc);

		SkEventDispatcher(pAC, pAC->IoBase);
		KeReleaseSpinLockFromDpcLevel(&pAC->SlowPathLock);
	}

#ifndef CONFIG_SK98LIN_NAPI
	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE);
#endif

	/* IRQ is processed - Enable IRQs again*/

#if 0
	NbDebugPrint(3, ("SkGeIsrOnePort: pAC->GIni.GIValIrqMask = %08X\n", pAC->GIni.GIValIrqMask));
	{	
		UCHAR Val8;		
		NbDebugPrint(3, ("SkGeIsrOnePort: pAC->TxPort[0][TX_PRIO_LOW].pTxdRingTail = %08X\n", pAC->TxPort[0][TX_PRIO_LOW].pTxdRingTail));

		SK_IN8(pAC->TxPort[0][TX_PRIO_LOW].HwAddr, Q_CSR, &Val8);
		NbDebugPrint(3, ("pAC->RxPort[0]:QCSR = %08X\n", Val8));

		SK_IN8(pAC->RxPort[0].HwAddr, Q_CSR, &Val8);
		NbDebugPrint(3, ("pAC->RxPort[0]:QCSR = %08X\n", Val8));
	}

	{
		ULONG Val32;

		SK_IN32(pAC->IoBase, TxQueueAddr[0][TX_PRIO_LOW]+ Q_DA_L,&Val32);
		NbDebugPrint(3, ("TxQueueAddr[0][TX_PRIO_LOW]+ Q_DA_L = %08X\n", Val32));
		SK_IN32(pAC->IoBase, TxQueueAddr[0][TX_PRIO_LOW]+ Q_DA_H,&Val32);
		NbDebugPrint(3, ("TxQueueAddr[0][TX_PRIO_LOW]+ Q_DA_H = %08X\n", Val32));
		SK_IN32(pAC->IoBase, RxQueueAddr[0]+Q_DA_L,	&Val32);
		NbDebugPrint(3, ("RxQueueAddr[0]+Q_DA_L = %08X\n", Val32));
		SK_IN32(pAC->IoBase, RxQueueAddr[0]+Q_DA_H, &Val32);
		NbDebugPrint(3, ("RxQueueAddr[0]+Q_DA_H = %08X\n", Val32));
	}

	{
		SK_U8 Byte;
		SK_IN8(pAC->IoBase, MR_ADDR(0, GMAC_CTRL), &Byte);
		NbDebugPrint(3, ("MR_ADDR(0, GMAC_CTRL) = %08X\n", Byte));
	}
#endif

	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);

	dev_xmit_all(NULL);

#ifndef __INTERRUPT__	
	deltaTime.QuadPart = - INTERRUPT_TIME;    
	(VOID) KeSetTimer( &SkGeInterruptTimer, deltaTime, &SkGeInterruptTimerDpc );
#endif	

	NbDebugPrint(3, ("SkGeIsrOnePort Exit\n"));

	return;
} /* SkGeIsrOnePort */

/****************************************************************************
 *
 *	SkGeOpen - handle start of initialized adapter
 *
 * Description:
 *	This function starts the initialized adapter.
 *	The board level variable is set and the adapter is
 *	brought to full functionality.
 *	The device flags are set for operation.
 *	Do all necessary level 2 initialization, enable interrupts and
 *	give start command to RLMT.
 *
 * Returns:
 *	0 on success
 *	!= 0 on error
 */
static int SkGeOpen(
struct SK_NET_DEVICE *dev)  /* the device that is to be opened */
{
	DEV_NET        *pNet = (DEV_NET*) dev->priv;
	SK_AC          *pAC  = pNet->pAC;
	KIRQL			Irql;    /* for the spin locks    */
	int             CurrMac;  /* loop ctr for ports    */
	LARGE_INTEGER deltaTime;
	ULONG RetryCount;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeOpen: pAC=0x%lX:\n", (unsigned long)pAC));

	if (pAC->DiagModeActive == DIAG_ACTIVE) {
		if (pAC->Pnmi.DiagAttached == SK_DIAG_RUNNING) {
			return (-1);   /* still in use by diag; deny actions */
		} 
	}


	/* Set blink mode */
	if ((pAC->PciDev->vendor == 0x1186) || (pAC->PciDev->vendor == 0x11ab ))
		pAC->GIni.GILedBlinkCtrl = OEM_CONFIG_VALUE;

	if (pAC->BoardLevel == SK_INIT_DATA) {
		/* level 1 init common modules here */
		if (SkGeInit(pAC, pAC->IoBase, SK_INIT_IO) != 0) {
			NbDebugPrint(0, ("%s: HWInit (1) failed.\n", pAC->dev[pNet->PortNr]->name));
			return (-1);
		}
		SkI2cInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkEventInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkPnmiInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkAddrInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkRlmtInit	(pAC, pAC->IoBase, SK_INIT_IO);
		SkTimerInit	(pAC, pAC->IoBase, SK_INIT_IO);
		pAC->BoardLevel = SK_INIT_IO;
#ifdef Y2_RECOVERY
		/* mark entries invalid */
		pAC->LastPort = 3;
		pAC->LastOpc = 0xFF;
#endif
	}

	if (pAC->BoardLevel != SK_INIT_RUN) {
		/* tschilling: Level 2 init modules here, check return value. */
		if (SkGeInit(pAC, pAC->IoBase, SK_INIT_RUN) != 0) {
			NbDebugPrint(0, ("%s: HWInit (2) failed.\n", pAC->dev[pNet->PortNr]->name));
			return (-1);
		}
		SkI2cInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkEventInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkPnmiInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkAddrInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkRlmtInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		SkTimerInit	(pAC, pAC->IoBase, SK_INIT_RUN);
		pAC->BoardLevel = SK_INIT_RUN;
	}

	for (CurrMac=0; CurrMac<pAC->GIni.GIMacsFound; CurrMac++) {
		if (!CHIP_ID_YUKON_2(pAC)) {
			/* Enable transmit descriptor polling. */
			SkGePollTxD(pAC, pAC->IoBase, CurrMac, SK_TRUE);
			FillRxRing(pAC, &pAC->RxPort[CurrMac]);
			SkMacRxTxEnable(pAC, pAC->IoBase, pNet->PortNr);
		}
	}

	SkGeYellowLED(pAC, pAC->IoBase, 1);
	SkDimEnableModerationIfNeeded(pAC);	

	if (!CHIP_ID_YUKON_2(pAC)) {
		/*
		** Has been setup already at SkGeInit(SK_INIT_IO),
		** but additional masking added for Genesis & Yukon
		** chipsets -> modify it...
		*/
		pAC->GIni.GIValIrqMask &= IRQ_MASK;
#ifndef USE_TX_COMPLETE
		pAC->GIni.GIValIrqMask &= ~(TX_COMPL_IRQS);
#endif
	}

	NbDebugPrint(3, ("SkGeOpen: pAC->GIni.GIValIrqMask = %08X\n", pAC->GIni.GIValIrqMask));
	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);

	
	NbDebugPrint(3, ("SkGeOpen: pAC->RlmtMode = %08X, pAC->MaxPorts = %d\n", pAC->RlmtMode, pAC->MaxPorts));
	if ((pAC->RlmtMode != 0) && (pAC->MaxPorts == 0)) {
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_SET_NETS,
					pAC->RlmtNets, -1, SK_FALSE);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_MODE_CHANGE,
					pAC->RlmtMode, 0, SK_FALSE);
	}

	SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_START,
				pNet->NetNr, -1, SK_TRUE);
	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);

	NbDebugPrint(3, ("SkGeOpen:SkLocalEventQueue After\n"));
#ifdef Y2_RECOVERY
	pNet->TimerExpired = SK_FALSE;
	pNet->InRecover = SK_FALSE;
	pNet->NetConsoleMode = SK_FALSE;

	/* Initialize the kernel timer */

/*
	init_timer(&pNet->KernelTimer);
	pNet->KernelTimer.function	= SkGeHandleKernelTimer;
	pNet->KernelTimer.data		= (unsigned long) pNet;
	pNet->KernelTimer.expires	= jiffies + (HZ/4); 
	add_timer(&pNet->KernelTimer);
*/

#ifndef __INTERRUPT__
	KeInitializeDpc( &pNet->KernelTimerDpc, SkGeHandleKernelTimer, pNet );
	KeInitializeTimer( &pNet->KernelTimer );
	deltaTime.QuadPart = - (HZ/4);
	KeSetTimer( &pNet->KernelTimer, deltaTime, &pNet->KernelTimerDpc );
#endif


#endif

	NbDebugPrint(3, ("SkGeOpen: enable Interrupts(B0_IMSK, B0_HWE_IMSK) \n"));
	/* enable Interrupts */

	pAC->GIni.GIValIrqMask = IS_ALL_MSK;

	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
	SK_OUT32(pAC->IoBase, B0_HWE_IMSK, IRQ_HWE_MASK);	

	NbDebugPrint(3, ("SkGeOpen: after enable Interrupts\n"));

#ifndef __INTERRUPT__
	/* Set the timer to switch to check for link beat and perhaps switch
	   to an alternate media type. */

	KeInitializeDpc( &SkGeInterruptTimerDpc, SkGeIsrOnePort, dev );
	KeInitializeTimer( &SkGeInterruptTimer );

	deltaTime.QuadPart = - INTERRUPT_TIME;    /* 2.4 sec. */
	KeSetTimer( &SkGeInterruptTimer, deltaTime, &SkGeInterruptTimerDpc );

#endif

	

	RetryCount = 0 ;
	while(RetryCount < 10) {		
		deltaTime.QuadPart = - HZ / 2;		
		KeDelayExecutionThread(KernelMode, FALSE, &deltaTime);
		NbDebugPrint(0, ("SkGeOpen: RetryCount = %d\n", RetryCount));
		RetryCount ++;		
		if(netif_carrier_ok(dev)) break;
	}	
	
	if(!netif_carrier_ok(dev)) {
		NbDebugPrint(0, ("SkGeOpen: Link is down\n"));
		SkGeClose(dev);
		return -EMLINK;
	}

	NbDebugPrint(0, ("SkGeOpen Exit\n"));
	
	pAC->MaxPorts++;
	MOD_INC_USE_COUNT;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeOpen suceeded\n"));

	return (0);
} /* SkGeOpen */


/****************************************************************************
 *
 *	SkGeClose - Stop initialized adapter
 *
 * Description:
 *	Close initialized adapter.
 *
 * Returns:
 *	0 - on success
 *	error code - on error
 */
static int SkGeClose(
struct SK_NET_DEVICE *dev)  /* the device that is to be closed */
{
	DEV_NET         *pNet = (DEV_NET*) dev->priv;
	SK_AC           *pAC  = pNet->pAC;
	DEV_NET         *newPtrNet;
	KIRQL			Irql;        /* for the spin locks           */
	int              CurrMac;      /* loop ctr for the current MAC */
	int              PortIdx;
#ifdef CONFIG_SK98LIN_NAPI
	int              WorkToDo = 1; /* min(*budget, dev->quota);    */
	int              WorkDone = 0;
#endif
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeClose: pAC=0x%lX ", (unsigned long)pAC));

#ifdef Y2_RECOVERY
	pNet->InRecover = SK_TRUE;
#ifndef __INTERRUPT__
	KeCancelTimer(&pNet->KernelTimer);
#endif
#endif

	if (pAC->DiagModeActive == DIAG_ACTIVE) {
		if (pAC->DiagFlowCtrl == SK_FALSE) {
			MOD_DEC_USE_COUNT;
			/* 
			** notify that the interface which has been closed
			** by operator interaction must not be started up 
			** again when the DIAG has finished. 
			*/
			newPtrNet = (DEV_NET *) pAC->dev[0]->priv;
			if (newPtrNet == pNet) {
				pAC->WasIfUp[0] = SK_FALSE;
			} else {
				pAC->WasIfUp[1] = SK_FALSE;
			}
			return 0; /* return to system everything is fine... */
		} else {
			pAC->DiagFlowCtrl = SK_FALSE;
		}
	}

	netif_stop_queue(dev);

	if (pAC->RlmtNets == 1)
		PortIdx = pAC->ActivePort;
	else
		PortIdx = pNet->NetNr;

	/*
	 * Clear multicast table, promiscuous mode ....
	 */
	SkAddrMcClear(pAC, pAC->IoBase, PortIdx, 0);
	SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
		SK_PROM_MODE_NONE);

	if (pAC->MaxPorts == 1) {
		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
		/* disable interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					pNet->NetNr, -1, SK_TRUE);
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		/* stop the hardware */


		if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 1)) {
		/* RLMT check link state mode */
			for (CurrMac=0; CurrMac<pAC->GIni.GIMacsFound; CurrMac++) {
				if (CHIP_ID_YUKON_2(pAC))
					SkY2PortStop(	pAC, 
							pAC->IoBase,
							CurrMac,
							SK_STOP_ALL,
							SK_HARD_RST);
				else
					SkGeStopPort(	pAC,
							pAC->IoBase,
							CurrMac,
							SK_STOP_ALL,
							SK_HARD_RST);
			} /* for */
		} else {
		/* Single link or single port */
			if (CHIP_ID_YUKON_2(pAC))
				SkY2PortStop(	pAC, 
						pAC->IoBase,
						PortIdx,
						SK_STOP_ALL,
						SK_HARD_RST);
			else
				SkGeStopPort(	pAC,
						pAC->IoBase,
						PortIdx,
						SK_STOP_ALL,
						SK_HARD_RST);
		}
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
	} else {
		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
		SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
					pNet->NetNr, -1, SK_FALSE);
		SkLocalEventQueue(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					pNet->NetNr, -1, SK_TRUE);
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
		
		/* Stop port */
		KeAcquireSpinLock(&pAC->TxPort[pNet->PortNr]
			[TX_PRIO_LOW].TxDesRingLock, &Irql);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, pAC->IoBase, pNet->PortNr,
				SK_STOP_ALL, SK_HARD_RST);
		}
		else {
			SkGeStopPort(pAC, pAC->IoBase, pNet->PortNr,
				SK_STOP_ALL, SK_HARD_RST);
		}
		KeReleaseSpinLock(&pAC->TxPort[pNet->PortNr]
			[TX_PRIO_LOW].TxDesRingLock, Irql);
	}

	if (pAC->RlmtNets == 1) {
		/* clear all descriptor rings */
		for (CurrMac=0; CurrMac<pAC->GIni.GIMacsFound; CurrMac++) {
			if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
				WorkToDo = 1;
				ReceiveIrq(pAC,&pAC->RxPort[CurrMac],
						SK_TRUE,&WorkDone,WorkToDo);
#else
				ReceiveIrq(pAC,&pAC->RxPort[CurrMac],SK_TRUE);
#endif
				ClearRxRing(pAC, &pAC->RxPort[CurrMac]);
				ClearTxRing(pAC, &pAC->TxPort[CurrMac][TX_PRIO_LOW]);
			} else {
				SkY2FreeRxBuffers(pAC, pAC->IoBase, CurrMac);
				SkY2FreeTxBuffers(pAC, pAC->IoBase, CurrMac);
			}
		}
	} else {
		/* clear port descriptor rings */
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE);
#endif
			ClearRxRing(pAC, &pAC->RxPort[pNet->PortNr]);
			ClearTxRing(pAC, &pAC->TxPort[pNet->PortNr][TX_PRIO_LOW]);
		}
		else {
			SkY2FreeRxBuffers(pAC, pAC->IoBase, pNet->PortNr);
			SkY2FreeTxBuffers(pAC, pAC->IoBase, pNet->PortNr);
		}
	}

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeClose: done "));

	SK_MEMSET(&(pAC->PnmiBackup), 0, sizeof(SK_PNMI_STRUCT_DATA));
	SK_MEMCPY(&(pAC->PnmiBackup), &(pAC->PnmiStruct), 
			sizeof(SK_PNMI_STRUCT_DATA));

	pAC->MaxPorts--;
	MOD_DEC_USE_COUNT;

#ifdef Y2_RECOVERY
	pNet->InRecover = SK_FALSE;
#endif

	sk98lin_remove_device(pAC->PciDev);

	return (0);
} /* SkGeClose */


/*****************************************************************************
 *
 * 	SkGeXmit - Linux frame transmit function
 *
 * Description:
 *	The system calls this function to send frames onto the wire.
 *	It puts the frame in the tx descriptor ring. If the ring is
 *	full then, the 'tbusy' flag is set.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 * WARNING: returning 1 in 'tbusy' case caused system crashes (double
 *	allocated skb's) !!!
 */
static int SkGeXmit(struct sk_buff *skb, struct SK_NET_DEVICE *dev)
{
DEV_NET		*pNet;
SK_AC		*pAC;
int			Rc;	/* return code of XmitFrame */

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;

	NbDebugPrint(3, ("SkGeXmit entered\n"));

	if ((!skb_shinfo(skb)->nr_frags) ||
		(pAC->GIni.GIChipId == CHIP_ID_GENESIS)) {
		/* Don't activate scatter-gather and hardware checksum */

		if (pAC->RlmtNets == 2)
			Rc = XmitFrame(
				pAC,
				&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW],
				skb);
		else
			Rc = XmitFrame(
				pAC,
				&pAC->TxPort[pAC->ActivePort][TX_PRIO_LOW],
				skb);
	} else {
		/* scatter-gather and hardware TCP checksumming anabled*/
		if (pAC->RlmtNets == 2)
			Rc = XmitFrameSG(
				pAC,
				&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW],
				skb);
		else
			Rc = XmitFrameSG(
				pAC,
				&pAC->TxPort[pAC->ActivePort][TX_PRIO_LOW],
				skb);
	}

	/* Transmitter out of resources? */
#ifdef USE_TX_COMPLETE
	if (Rc <= 0) {
		netif_stop_queue(dev);
	}
#endif

	/* If not taken, give buffer ownership back to the
	 * queueing layer.
	 */
	if (Rc < 0)
		return (1);

	dev->trans_start = jiffies;
	return (0);
} /* SkGeXmit */

#ifdef CONFIG_SK98LIN_NAPI
/*****************************************************************************
 *
 * 	SkGePoll - NAPI Rx polling callback for GEnesis and Yukon chipsets
 *
 * Description:
 *	Called by the Linux system in case NAPI polling is activated
 *
 * Returns:
 *	The number of work data still to be handled
 */
static int SkGePoll(struct net_device *dev, int *budget) 
{
	SK_AC		*pAC = ((DEV_NET*)(dev->priv))->pAC; /* pointer to adapter context */
	int		WorkToDo = min(*budget, dev->quota);
	int		WorkDone = 0;
	KIRQL	Irql;       


	if (pAC->dev[0] != pAC->dev[1]) {
#ifdef USE_TX_COMPLETE
		KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
		FreeTxDescriptors(pAC, &pAC->TxPort[1][TX_PRIO_LOW]);
		KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[1][TX_PRIO_LOW].TxDesRingLock);
#endif
		ReceiveIrq(pAC, &pAC->RxPort[1], SK_TRUE, &WorkDone, WorkToDo);
		CLEAR_AND_START_RX(1);
	}
#ifdef USE_TX_COMPLETE
	KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
	FreeTxDescriptors(pAC, &pAC->TxPort[0][TX_PRIO_LOW]);
	KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[0][TX_PRIO_LOW].TxDesRingLock);
#endif
	ReceiveIrq(pAC, &pAC->RxPort[0], SK_TRUE, &WorkDone, WorkToDo);
	CLEAR_AND_START_RX(0);

	*budget -= WorkDone;
	dev->quota -= WorkDone;

	if(WorkDone < WorkToDo) {
		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
		netif_rx_complete(dev);
		pAC->GIni.GIValIrqMask |= (NAPI_DRV_IRQS);
#ifndef USE_TX_COMPLETE
		pAC->GIni.GIValIrqMask &= ~(TX_COMPL_IRQS);
#endif
		/* enable interrupts again */
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
	}
	return (WorkDone >= WorkToDo);
} /* SkGePoll */
#endif

#ifdef SK_POLL_CONTROLLER
/*****************************************************************************
 *
 * 	SkGeNetPoll - Polling "interrupt"
 *
 * Description:
 *	Polling 'interrupt' - used by things like netconsole and netdump
 *	to send skbs without having to re-enable interrupts.
 *	It's not called while the interrupt routine is executing.
 */
static void SkGeNetPoll(
struct SK_NET_DEVICE *dev) 
{
DEV_NET		*pNet;
SK_AC		*pAC;

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	pNet->NetConsoleMode = SK_TRUE;

	if (unlikely(netdump_mode)) {
		int bogus_budget = 64;

		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2Isr(dev->irq, dev, NULL);
			if (dev->poll_list.prev)
				SkY2Poll(dev, &bogus_budget);
		} else {
			if (pAC->GIni.GIMacsFound == 2)
				SkGeIsr(dev->irq, dev, NULL);
			else
				SkGeIsrOnePort(dev->irq, dev, NULL);
			if (dev->poll_list.prev)
				SkGePoll(dev, &bogus_budget);
		}


	} else {			
		/*  Prevent any reconfiguration while handling
		    the 'interrupt' */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);

		if (!CHIP_ID_YUKON_2(pAC)) {
		/* Handle the GENESIS Isr */
			if (pAC->GIni.GIMacsFound == 2)
				SkGeIsr(dev->irq, dev, NULL);
			else
				SkGeIsrOnePort(dev->irq, dev, NULL);
		} else {
		/* Handle the Yukon2 Isr */
			SkY2Isr(dev->irq, dev, NULL);
		}

	}
}
#endif

SK_U64 ring_ptr;

static int SkGe_get_status(struct SK_NET_DEVICE *dev)
{
	DEV_NET		*pNet;
	SK_AC		*pAC;		
	int PortIndex  = 0;	
	SK_U32 Val32;

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;	
	
	SK_IN32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_H,&Val32);
	ring_ptr = (SK_U64) (Val32) << 32;
	SK_IN32(pAC->IoBase, TxQueueAddr[PortIndex][TX_PRIO_LOW]+ Q_DA_L,&Val32);
	ring_ptr = ring_ptr + (SK_U64) Val32;
	
	if(  (ring_ptr < pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing) 
		|| (ring_ptr > pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing + TX_RING_SIZE) 
		|| (ring_ptr % DESCR_ALIGN != pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing % DESCR_ALIGN) 
		) {
		NbDebugPrint (0, ("skge status is corrupted, ring_ptr = %08X%08X, VTxDescrRing = %08X%08X\n",
			(ULONG)(ring_ptr >> 32),
			(ULONG)(ring_ptr & 0xFFFFFFFF),
			(ULONG)(pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing >> 32),
			(ULONG)(pAC->TxPort[PortIndex][TX_PRIO_LOW].VTxDescrRing & 0xFFFFFFFF)
		));		
		
		NbDebugLevel = 3;

		return STATUS_NIC_CORRUPTED;
	}	
	
	return STATUS_NIC_OK ;
}

/*****************************************************************************
 *
 * 	XmitFrame - fill one socket buffer into the transmit ring
 *
 * Description:
 *	This function puts a message into the transmit descriptor ring
 *	if there is a descriptors left.
 *	Linux skb's consist of only one continuous buffer.
 *	The first step locks the ring. It is held locked
 *	all time to avoid problems with SWITCH_../PORT_RESET.
 *	Then the descriptoris allocated.
 *	The second part is linking the buffer to the descriptor.
 *	At the very last, the Control field of the descriptor
 *	is made valid for the BMU and a start TX command is given
 *	if necessary.
 *
 * Returns:
 *	> 0 - on succes: the number of bytes in the message
 *	= 0 - on resource shortage: this frame sent or dropped, now
 *		the ring is full ( -> set tbusy)
 *	< 0 - on failure: other problems ( -> return failure to upper layers)
 */
struct sk_buff orig, padding;

static int XmitFrame(
SK_AC 		*pAC,		/* pointer to adapter context	        */
TX_PORT		*pTxPort,	/* pointer to struct of port to send to */
struct sk_buff	*pMessage)	/* pointer to send-message              */
{
	TXD		*pTxd;		/* the rxd to fill */
	TXD		*pOldTxd;
	KIRQL	Irql;
	SK_U64		 PhysAddr;
	int	 	 Protocol;
	int		 IpHeaderLength;
	int		 BytesSend = pMessage->len;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_TX_PROGRESS, ("X"));
	NbDebugPrint(3,("XmitFrame\n"));

	KeAcquireSpinLock(&pTxPort->TxDesRingLock, &Irql);
#ifndef USE_TX_COMPLETE
	if ((pTxPort->TxdRingPrevFree - pTxPort->TxdRingFree) > 6)  {
		FreeTxDescriptors(pAC, pTxPort);
		pTxPort->TxdRingPrevFree = pTxPort->TxdRingFree;
	}
#endif
	if (pTxPort->TxdRingFree == 0) {
		/* 
		** not enough free descriptors in ring at the moment.
		** Maybe free'ing some old one help?
		*/
		FreeTxDescriptors(pAC, pTxPort);
		if (pTxPort->TxdRingFree == 0) {
			KeReleaseSpinLock(&pTxPort->TxDesRingLock, Irql);
			SK_PNMI_CNT_NO_TX_BUF(pAC, pTxPort->PortIndex);
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_TX_PROGRESS,
				("XmitFrame failed\n"));
			/* 
			** the desired message can not be sent
			** Because tbusy seems to be set, the message 
			** should not be freed here. It will be used 
			** by the scheduler of the ethernet handler 
			*/
			return (-1);
		}
	}

	/*
	** If the passed socket buffer is of smaller MTU-size than 60,
	** copy everything into new buffer and fill all bytes between
	** the original packet end and the new packet end of 60 with 0x00.
	** This is to resolve faulty padding by the HW with 0xaa bytes.
	*/

	memcpy((void*)(&orig), (void *)pMessage, sizeof(*pMessage));
	if (BytesSend < C_LEN_ETHERNET_MINSIZE) {
		if ((pMessage = skb_padto(pMessage, C_LEN_ETHERNET_MINSIZE)) == NULL) {
			KeReleaseSpinLock(&pTxPort->TxDesRingLock, Irql);
			return 0;
		}
		pMessage->len = C_LEN_ETHERNET_MINSIZE;
	}
	memcpy((void*)(&padding), (void *)pMessage, sizeof(*pMessage));

	/* 
	** advance head counter behind descriptor needed for this frame, 
	** so that needed descriptor is reserved from that on. The next
	** action will be to add the passed buffer to the TX-descriptor
	*/
	pTxd = pTxPort->pTxdRingHead;
	pTxPort->pTxdRingHead = pTxd->pNextTxd;
	pTxPort->TxdRingFree--;

#ifdef SK_DUMP_TX
	DumpMsg(pMessage, "XmitFrame");
#endif

	/* 
	** First step is to map the data to be sent via the adapter onto
	** the DMA memory. Kernel 2.2 uses virt_to_bus(), but kernels 2.4
	** and 2.6 need to use pci_map_page() for that mapping.
	*/
	/*
	PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
					virt_to_page(pMessage->data),
					((unsigned long) pMessage->data & ~PAGE_MASK),
					pMessage->len,
					PCI_DMA_TODEVICE);
	*/
	PhysAddr = (SK_U64) virt_to_bus(pMessage->data);  // correct?

	pTxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
	pTxd->VDataHigh = (SK_U32) (PhysAddr >> 32);
	pTxd->pMBuf     = pMessage;

	if (pMessage->ip_summed == CHECKSUM_HW) {
		Protocol = ((SK_U8)pMessage->data[C_OFFSET_IPPROTO] & 0xff);
		if ((Protocol == C_PROTO_ID_UDP) && 
			(pAC->GIni.GIChipRev == 0) &&
			(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
			pTxd->TBControl = BMU_TCP_CHECK;
		} else {
			pTxd->TBControl = BMU_UDP_CHECK;
		}

		IpHeaderLength  = (SK_U8)pMessage->data[C_OFFSET_IPHEADER];
		IpHeaderLength  = (IpHeaderLength & 0xf) * 4;
		pTxd->TcpSumOfs = 0; /* PH-Checksum already calculated */
		pTxd->TcpSumSt  = C_LEN_ETHERMAC_HEADER + IpHeaderLength + 
							(Protocol == C_PROTO_ID_UDP ?
							C_OFFSET_UDPHEADER_UDPCS : 
							C_OFFSET_TCPHEADER_TCPCS);
		pTxd->TcpSumWr  = C_LEN_ETHERMAC_HEADER + IpHeaderLength;

		pTxd->TBControl |= BMU_OWN | BMU_STF | 
				   BMU_SW  | BMU_EOF |
#ifdef USE_TX_COMPLETE
				   BMU_IRQ_EOF |
#endif
				   pMessage->len;
	} else {
		pTxd->TBControl = BMU_OWN | BMU_STF | BMU_CHECK | 
				  BMU_SW  | BMU_EOF |
#ifdef USE_TX_COMPLETE
				   BMU_IRQ_EOF |
#endif
			pMessage->len;
	}

	NbDebugPrint(3,("XmitFrame: pTxd = %08X, pTxd->TBControl = %08X, pMessage = %08X\n", pTxd, pTxd->TBControl, pMessage));

	/* 
	** If previous descriptor already done, give TX start cmd 
	*/
//	pOldTxd = xchg(&pTxPort->pTxdRingPrev, pTxd);
	pOldTxd = pTxPort->pTxdRingPrev;
	pTxPort->pTxdRingPrev = pTxd;	

	if ((pOldTxd->TBControl & BMU_OWN) == 0) {
		SK_OUT8(pTxPort->HwAddr, Q_CSR, CSR_START);
	}	

	/* 
	** after releasing the lock, the skb may immediately be free'd 
	*/
	KeReleaseSpinLock(&pTxPort->TxDesRingLock, Irql);
	
	if (pTxPort->TxdRingFree != 0) {
		return (BytesSend);
	} else {
		return (0);
	}

} /* XmitFrame */

/*****************************************************************************
 *
 * 	XmitFrameSG - fill one socket buffer into the transmit ring
 *                (use SG and TCP/UDP hardware checksumming)
 *
 * Description:
 *	This function puts a message into the transmit descriptor ring
 *	if there is a descriptors left.
 *
 * Returns:
 *	> 0 - on succes: the number of bytes in the message
 *	= 0 - on resource shortage: this frame sent or dropped, now
 *		the ring is full ( -> set tbusy)
 *	< 0 - on failure: other problems ( -> return failure to upper layers)
 */
static int XmitFrameSG(
SK_AC 		*pAC,		/* pointer to adapter context           */
TX_PORT		*pTxPort,	/* pointer to struct of port to send to */
struct sk_buff	*pMessage)	/* pointer to send-message              */
{

	TXD		*pTxd;
	TXD		*pTxdFst;
	TXD		*pTxdLst;
	unsigned int	CurrFrag;
	int		 BytesSend;
	int		 IpHeaderLength; 
	int		 Protocol;
	skb_frag_t	*sk_frag;
	SK_U64		 PhysAddr;
	KIRQL	Irql;

	NbDebugPrint(0, ("XmitFrameSG\n"));
	KeAcquireSpinLock(&pTxPort->TxDesRingLock, &Irql);
#ifndef USE_TX_COMPLETE
	FreeTxDescriptors(pAC, pTxPort);
#endif
	if ((skb_shinfo(pMessage)->nr_frags +1) > (unsigned)pTxPort->TxdRingFree) {
		FreeTxDescriptors(pAC, pTxPort);
		if ((skb_shinfo(pMessage)->nr_frags + 1) > (unsigned)pTxPort->TxdRingFree) {
			KeReleaseSpinLock(&pTxPort->TxDesRingLock, Irql);
			SK_PNMI_CNT_NO_TX_BUF(pAC, pTxPort->PortIndex);
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_TX_PROGRESS,
				("XmitFrameSG failed - Ring full\n"));
				/* this message can not be sent now */
			return(-1);
		}
	}

	pTxd      = pTxPort->pTxdRingHead;
	pTxdFst   = pTxd;
	pTxdLst   = pTxd;
	BytesSend = 0;
	Protocol  = 0;

	/* 
	** Map the first fragment (header) into the DMA-space
	*/
	/*
	PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
			virt_to_page(pMessage->data),
			((unsigned long) pMessage->data & ~PAGE_MASK),
			skb_headlen(pMessage),
			PCI_DMA_TODEVICE);
	*/
	PhysAddr = (SK_U64) virt_to_bus(pMessage->data);

	pTxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
	pTxd->VDataHigh = (SK_U32) (PhysAddr >> 32);

	/* 
	** Does the HW need to evaluate checksum for TCP or UDP packets? 
	*/
	if (pMessage->ip_summed == CHECKSUM_HW) {
		pTxd->TBControl = BMU_STF | BMU_STFWD | skb_headlen(pMessage);
		/* 
		** We have to use the opcode for tcp here,  because the
		** opcode for udp is not working in the hardware yet 
		** (Revision 2.0)
		*/
		Protocol = ((SK_U8)pMessage->data[C_OFFSET_IPPROTO] & 0xff);
		if ((Protocol == C_PROTO_ID_UDP) && 
			(pAC->GIni.GIChipRev == 0) &&
			(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
			pTxd->TBControl |= BMU_TCP_CHECK;
		} else {
			pTxd->TBControl |= BMU_UDP_CHECK;
		}

		IpHeaderLength  = ((SK_U8)pMessage->data[C_OFFSET_IPHEADER] & 0xf)*4;
		pTxd->TcpSumOfs = 0; /* PH-Checksum already claculated */
		pTxd->TcpSumSt  = C_LEN_ETHERMAC_HEADER + IpHeaderLength +
						(Protocol == C_PROTO_ID_UDP ?
						C_OFFSET_UDPHEADER_UDPCS :
						C_OFFSET_TCPHEADER_TCPCS);
		pTxd->TcpSumWr  = C_LEN_ETHERMAC_HEADER + IpHeaderLength;
	} else {
		pTxd->TBControl = BMU_CHECK | BMU_SW | BMU_STF |
					skb_headlen(pMessage);
	}

	pTxd = pTxd->pNextTxd;
	pTxPort->TxdRingFree--;
	BytesSend += skb_headlen(pMessage);

	/* 
	** Browse over all SG fragments and map each of them into the DMA space
	*/
	for (CurrFrag = 0; CurrFrag < skb_shinfo(pMessage)->nr_frags; CurrFrag++) {
		sk_frag = &skb_shinfo(pMessage)->frags[CurrFrag];
		/* 
		** we already have the proper value in entry
		*/
/*		PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
						 sk_frag->page,
						 sk_frag->page_offset,
						 sk_frag->size,
						 PCI_DMA_TODEVICE);
*/
		PhysAddr = (SK_U64) virt_to_bus(pMessage->data);

		pTxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
		pTxd->VDataHigh = (SK_U32) (PhysAddr >> 32);
		pTxd->pMBuf     = pMessage;
		
		/* 
		** Does the HW need to evaluate checksum for TCP or UDP packets? 
		*/
		if (pMessage->ip_summed == CHECKSUM_HW) {
			pTxd->TBControl = BMU_OWN | BMU_SW | BMU_STFWD;
			/* 
			** We have to use the opcode for tcp here because the 
			** opcode for udp is not working in the hardware yet 
			** (revision 2.0)
			*/
			if ((Protocol == C_PROTO_ID_UDP) && 
				(pAC->GIni.GIChipRev == 0) &&
				(pAC->GIni.GIChipId == CHIP_ID_YUKON)) {
				pTxd->TBControl |= BMU_TCP_CHECK;
			} else {
				pTxd->TBControl |= BMU_UDP_CHECK;
			}
		} else {
			pTxd->TBControl = BMU_CHECK | BMU_SW | BMU_OWN;
		}

		/* 
		** Do we have the last fragment? 
		*/
		if( (CurrFrag+1) == skb_shinfo(pMessage)->nr_frags )  {
#ifdef USE_TX_COMPLETE
			pTxd->TBControl |= BMU_EOF | BMU_IRQ_EOF | sk_frag->size;
#else
			pTxd->TBControl |= BMU_EOF | sk_frag->size;
#endif
			pTxdFst->TBControl |= BMU_OWN | BMU_SW;

		} else {
			pTxd->TBControl |= sk_frag->size;
		}
		pTxdLst = pTxd;
		pTxd    = pTxd->pNextTxd;
		pTxPort->TxdRingFree--;
		BytesSend += sk_frag->size;
	}

	/* 
	** If previous descriptor already done, give TX start cmd 
	*/
	if ((pTxPort->pTxdRingPrev->TBControl & BMU_OWN) == 0) {
		SK_OUT8(pTxPort->HwAddr, Q_CSR, CSR_START);
	}

	pTxPort->pTxdRingPrev = pTxdLst;
	pTxPort->pTxdRingHead = pTxd;

	KeReleaseSpinLock(&pTxPort->TxDesRingLock, Irql);

	if (pTxPort->TxdRingFree > 0) {
		return (BytesSend);
	} else {
		return (0);
	}
}

/*****************************************************************************
 *
 * 	FreeTxDescriptors - release descriptors from the descriptor ring
 *
 * Description:
 *	This function releases descriptors from a transmit ring if they
 *	have been sent by the BMU.
 *	If a descriptors is sent, it can be freed and the message can
 *	be freed, too.
 *	The SOFTWARE controllable bit is used to prevent running around a
 *	completely free ring for ever. If this bit is no set in the
 *	frame (by XmitFrame), this frame has never been sent or is
 *	already freed.
 *	The Tx descriptor ring lock must be held while calling this function !!!
 *
 * Returns:
 *	none
 */
TXD	*tempTxd;

static void FreeTxDescriptors(
SK_AC	*pAC,		/* pointer to the adapter context */
TX_PORT	*pTxPort)	/* pointer to destination port structure */
{
TXD	*pTxd;		/* pointer to the checked descriptor */
TXD	*pNewTail;	/* pointer to 'end' of the ring */
SK_U32	Control;	/* TBControl field of descriptor */
SK_U64	PhysAddr;	/* address of DMA mapping */

	pNewTail = pTxPort->pTxdRingTail;
	pTxd     = pNewTail;
	/*
	** loop forever; exits if BMU_SW bit not set in start frame
	** or BMU_OWN bit set in any frame
	*/
	while (1) {
		Control = pTxd->TBControl;
		NbDebugPrint(3, ("FreeTxDescriptors: Control = %08X\n", Control));
		if ((Control & BMU_SW) == 0) {
			/*
			** software controllable bit is set in first
			** fragment when given to BMU. Not set means that
			** this fragment was never sent or is already
			** freed ( -> ring completely free now).
			*/
			pTxPort->pTxdRingTail = pTxd;
			netif_wake_queue(pAC->dev[pTxPort->PortIndex]);
			return;
		}
		if (Control & BMU_OWN) {
			pTxPort->pTxdRingTail = pTxd;
			if (pTxPort->TxdRingFree > 0) {
				netif_wake_queue(pAC->dev[pTxPort->PortIndex]);
			}
			return;
		}
		
		/* 
		** release the DMA mapping, because until not unmapped
		** this buffer is considered being under control of the
		** adapter card!
		*/
		PhysAddr = ((SK_U64) pTxd->VDataHigh) << (SK_U64) 32;
		PhysAddr |= (SK_U64) pTxd->VDataLow;
/*		pci_unmap_page(pAC->PciDev, PhysAddr,
				 pTxd->pMBuf->len,
				 PCI_DMA_TODEVICE);
*/
		if (Control & BMU_EOF) {
			tempTxd = pTxd;
			DEV_KFREE_SKB_ANY(pTxd->pMBuf);	/* free message */
		}

		pTxPort->TxdRingFree++;
		pTxd->TBControl &= ~BMU_SW;
		pTxd = pTxd->pNextTxd; /* point behind fragment with EOF */
	} /* while(forever) */
} /* FreeTxDescriptors */

/*****************************************************************************
 *
 * 	FillRxRing - fill the receive ring with valid descriptors
 *
 * Description:
 *	This function fills the receive ring descriptors with data
 *	segments and makes them valid for the BMU.
 *	The active ring is filled completely, if possible.
 *	The non-active ring is filled only partial to save memory.
 *
 * Description of rx ring structure:
 *	head - points to the descriptor which will be used next by the BMU
 *	tail - points to the next descriptor to give to the BMU
 *	
 * Returns:	N/A
 */
static void FillRxRing(
SK_AC		*pAC,		/* pointer to the adapter context */
RX_PORT		*pRxPort)	/* ptr to port struct for which the ring
				   should be filled */
{
KIRQL	Irql;

	NbDebugPrint(3, ("FillRxRing\n"));
	KeAcquireSpinLock(&pRxPort->RxDesRingLock, &Irql);
	while (pRxPort->RxdRingFree > pRxPort->RxFillLimit) {
		if(!FillRxDescriptor(pAC, pRxPort))
			break;
	}
	KeReleaseSpinLock(&pRxPort->RxDesRingLock, Irql);
} /* FillRxRing */


/*****************************************************************************
 *
 * 	FillRxDescriptor - fill one buffer into the receive ring
 *
 * Description:
 *	The function allocates a new receive buffer and
 *	puts it into the next descriptor.
 *
 * Returns:
 *	SK_TRUE - a buffer was added to the ring
 *	SK_FALSE - a buffer could not be added
 */
static SK_BOOL FillRxDescriptor(
SK_AC		*pAC,		/* pointer to the adapter context struct */
RX_PORT		*pRxPort)	/* ptr to port struct of ring to fill */
{
struct sk_buff	*pMsgBlock;	/* pointer to a new message block */
RXD		*pRxd;		/* the rxd to fill */
SK_U16		Length;		/* data fragment length */
SK_U64		PhysAddr;	/* physical address of a rx buffer */

	NbDebugPrint(3, ("FillRxDescriptor\n"));
	pMsgBlock = alloc_skb(pRxPort->RxBufSize, 0);
	if (pMsgBlock == NULL) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
			SK_DBGCAT_DRV_ENTRY,
			("%s: Allocation of rx buffer failed !\n",
			pAC->dev[pRxPort->PortIndex]->name));
		SK_PNMI_CNT_NO_RX_BUF(pAC, pRxPort->PortIndex);
		return(SK_FALSE);
	}
	skb_reserve(pMsgBlock, 2); /* to align IP frames */
	/* skb allocated ok, so add buffer */
	pRxd = pRxPort->pRxdRingTail;
	pRxPort->pRxdRingTail = pRxd->pNextRxd;
	pRxPort->RxdRingFree--;
	Length = (USHORT)pRxPort->RxBufSize;

/*
	PhysAddr = (SK_U64) pci_map_page(pAC->PciDev,
		virt_to_page(pMsgBlock->data),
		((unsigned long) pMsgBlock->data &
		~PAGE_MASK),
		pRxPort->RxBufSize - 2,
		PCI_DMA_FROMDEVICE);

*/
	PhysAddr = (SK_U64) virt_to_bus(pMsgBlock->data);	

	pRxd->VDataLow  = (SK_U32) (PhysAddr & 0xffffffff);
	pRxd->VDataHigh = (SK_U32) (PhysAddr >> 32);
	pRxd->pMBuf     = pMsgBlock;
	pRxd->RBControl = BMU_OWN       | 
			  BMU_STF       | 
			  BMU_IRQ_EOF   | 
			  BMU_TCP_CHECK | 
			  Length;

	NbDebugPrint(3, ("FillRxDescriptor: pMsgBlock->data = %08X, pRxd->VDataLow = %08X, pRxd->VDataHigh = %08X, pRxd->pMBuf = %08X, pRxd->RBControl = %08X\n", 
			pMsgBlock->data,
			pRxd->VDataLow,
			pRxd->VDataHigh, 
			pRxd->pMBuf,
			pRxd->RBControl));

	return (SK_TRUE);

} /* FillRxDescriptor */


/*****************************************************************************
 *
 * 	ReQueueRxBuffer - fill one buffer back into the receive ring
 *
 * Description:
 *	Fill a given buffer back into the rx ring. The buffer
 *	has been previously allocated and aligned, and its phys.
 *	address calculated, so this is no more necessary.
 *
 * Returns: N/A
 */
static void ReQueueRxBuffer(
SK_AC		*pAC,		/* pointer to the adapter context struct */
RX_PORT		*pRxPort,	/* ptr to port struct of ring to fill */
struct sk_buff	*pMsg,		/* pointer to the buffer */
SK_U32		PhysHigh,	/* phys address high dword */
SK_U32		PhysLow)	/* phys address low dword */
{
RXD		*pRxd;		/* the rxd to fill */
SK_U16		Length;		/* data fragment length */

	pRxd = pRxPort->pRxdRingTail;
	pRxPort->pRxdRingTail = pRxd->pNextRxd;
	pRxPort->RxdRingFree--;
	Length = (USHORT)pRxPort->RxBufSize;

	pRxd->VDataLow  = PhysLow;
	pRxd->VDataHigh = PhysHigh;
	pRxd->pMBuf     = pMsg;
	pRxd->RBControl = BMU_OWN       | 
			  BMU_STF       |
			  BMU_IRQ_EOF   | 
			  BMU_TCP_CHECK | 
			  Length;
	return;
} /* ReQueueRxBuffer */

/*****************************************************************************
 *
 * 	ReceiveIrq - handle a receive IRQ
 *
 * Description:
 *	This function is called when a receive IRQ is set.
 *	It walks the receive descriptor ring and sends up all
 *	frames that are complete.
 *
 * Returns:	N/A
 */
static void ReceiveIrq(
#ifdef CONFIG_SK98LIN_NAPI
SK_AC    *pAC,          /* pointer to adapter context          */
RX_PORT  *pRxPort,      /* pointer to receive port struct      */
SK_BOOL   SlowPathLock, /* indicates if SlowPathLock is needed */
int      *WorkDone,
int       WorkToDo)
#else
SK_AC    *pAC,          /* pointer to adapter context          */
RX_PORT  *pRxPort,      /* pointer to receive port struct      */
SK_BOOL   SlowPathLock) /* indicates if SlowPathLock is needed */

#endif
{
	RXD             *pRxd;          /* pointer to receive descriptors         */
	struct sk_buff  *pMsg;          /* pointer to message holding frame       */
	struct sk_buff  *pNewMsg;       /* pointer to new message for frame copy  */
	SK_MBUF         *pRlmtMbuf;     /* ptr to buffer for giving frame to RLMT */
	SK_EVPARA        EvPara;        /* an event parameter union        */	
	SK_U32           Control;       /* control field of descriptor     */
	KIRQL			 Irql;         /* for spin lock handling          */
	int              PortIndex = pRxPort->PortIndex;
	int              FrameLength;   /* total length of received frame  */
	int              IpFrameLength; /* IP length of the received frame */
	unsigned int     Offset;
	unsigned int     NumBytes;
	unsigned int     RlmtNotifier;
	SK_BOOL          IsBc;          /* we received a broadcast packet  */
	SK_BOOL          IsMc;          /* we received a multicast packet  */
	SK_BOOL          IsBadFrame;    /* the frame received is bad!      */
	SK_U32           FrameStat;
	unsigned short   Csum1;
	unsigned short   Csum2;
	unsigned short   Type;
	int              Result;
	SK_U64           PhysAddr;

rx_start:	
	/* do forever; exit if BMU_OWN found */
	for ( pRxd = pRxPort->pRxdRingHead ;
		  pRxPort->RxdRingFree < pAC->RxDescrPerRing ;
		  pRxd = pRxd->pNextRxd,
		  pRxPort->pRxdRingHead = pRxd,
		  pRxPort->RxdRingFree ++) {

		/*
		 * For a better understanding of this loop
		 * Go through every descriptor beginning at the head
		 * Please note: the ring might be completely received so the OWN bit
		 * set is not a good crirteria to leave that loop.
		 * Therefore the RingFree counter is used.
		 * On entry of this loop pRxd is a pointer to the Rxd that needs
		 * to be checked next.
		 */

		Control = pRxd->RBControl;
	
#ifdef CONFIG_SK98LIN_NAPI
		if (*WorkDone >= WorkToDo) {
			break;
		}
		(*WorkDone)++;
#endif

		NbDebugPrint(3, ("ReceiveIrq: Control = %08X\n", Control));

		/* check if this descriptor is ready */
		if ((Control & BMU_OWN) != 0) {
			/* this descriptor is not yet ready */
			/* This is the usual end of the loop */
			/* We don't need to start the ring again */
			FillRxRing(pAC, pRxPort);
			return;
		}

		/* get length of frame and check it */
		FrameLength = Control & BMU_BBC;
		if (FrameLength > pRxPort->RxBufSize) {
			goto rx_failed;
		}

		/* check for STF and EOF */
		if ((Control & (BMU_STF | BMU_EOF)) != (BMU_STF | BMU_EOF)) {
			goto rx_failed;
		}

		/* here we have a complete frame in the ring */
		pMsg = pRxd->pMBuf;

		FrameStat = pRxd->FrameStat;

		/* check for frame length mismatch */
#define XMR_FS_LEN_SHIFT	18
#define GMR_FS_LEN_SHIFT	16
		if (pAC->GIni.GIChipId == CHIP_ID_GENESIS) {
			if (FrameLength != (SK_U32) (FrameStat >> XMR_FS_LEN_SHIFT)) {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,
					("skge: Frame length mismatch (%u/%u).\n",
					FrameLength,
					(SK_U32) (FrameStat >> XMR_FS_LEN_SHIFT)));
				goto rx_failed;
			}
		} else {
			if (FrameLength != (SK_U32) (FrameStat >> GMR_FS_LEN_SHIFT)) {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,
					("skge: Frame length mismatch (%u/%u).\n",
					FrameLength,
					(SK_U32) (FrameStat >> XMR_FS_LEN_SHIFT)));
				goto rx_failed;
			}
		}

		/* Set Rx Status */
		if (pAC->GIni.GIChipId == CHIP_ID_GENESIS) {
			IsBc = (FrameStat & XMR_FS_BC) != 0;
			IsMc = (FrameStat & XMR_FS_MC) != 0;
			IsBadFrame = (FrameStat &
				(XMR_FS_ANY_ERR | XMR_FS_2L_VLAN)) != 0;
		} else {
			IsBc = (FrameStat & GMR_FS_BC) != 0;
			IsMc = (FrameStat & GMR_FS_MC) != 0;
			IsBadFrame = (((FrameStat & GMR_FS_ANY_ERR) != 0) ||
							((FrameStat & GMR_FS_RX_OK) == 0));
		}

//		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, 0,
//			("Received frame of length %d on port %d\n",
//			FrameLength, PortIndex));
//		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, 0,
//			("Number of free rx descriptors: %d\n",
//			pRxPort->RxdRingFree));

//		pMsg->len = FrameLength;

//	 DumpMsg(pMsg, "Rx");

		if ((Control & BMU_STAT_VAL) != BMU_STAT_VAL || (IsBadFrame)) {
			/* there is a receive error in this frame */
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_RX_PROGRESS,
				("skge: Error in received frame, dropped!\n"
				"Control: %x\nRxStat: %x\n",
				Control, FrameStat));

			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;

			pci_dma_sync_single(pAC->PciDev,
						(dma_addr_t) PhysAddr,
						FrameLength,
						PCI_DMA_FROMDEVICE); // correct ?

			ReQueueRxBuffer(pAC, pRxPort, pMsg,
				pRxd->VDataHigh, pRxd->VDataLow);

			continue;
		}

		/*
		 * if short frame then copy data to reduce memory waste
		 */
		if ((FrameLength < SK_COPY_THRESHOLD) &&
			((pNewMsg = alloc_skb(FrameLength+2, 0)) != NULL)) {
			/*
			 * Short frame detected and allocation successfull
			 */
			/* use new skb and copy data */
			NbDebugPrint(3,("Short Frame: FrameLenth = %d\n", FrameLength));

			skb_reserve(pNewMsg, 2);
			skb_put(pNewMsg, FrameLength);
			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;

			pci_dma_sync_single(pAC->PciDev,
						(dma_addr_t) PhysAddr,
						FrameLength,
						PCI_DMA_FROMDEVICE); // correct ?

			NbDebugPrint(3,("Short Frame2: FrameLenth = %d\n", FrameLength));

			eth_copy_and_sum(pNewMsg, pMsg->data,
				FrameLength, 0);
			ReQueueRxBuffer(pAC, pRxPort, pMsg,
				pRxd->VDataHigh, pRxd->VDataLow);

			pMsg = pNewMsg;

		} else {
			/*
			 * if large frame, or SKB allocation failed, pass
			 * the SKB directly to the networking
			 */
			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;

			/* release the DMA mapping */
			/*
			pci_unmap_single(pAC->PciDev,
					 PhysAddr,
					 pRxPort->RxBufSize - 2,
					 PCI_DMA_FROMDEVICE);
			 */
			skb_put(pMsg, FrameLength); /* set message len */
			pMsg->ip_summed = CHECKSUM_NONE; /* initial default */

			if (pRxPort->UseRxCsum) {
				Type = NTOHS(*((short*)&pMsg->data[12]));
				if (Type == 0x800) {
					IpFrameLength = (int) NTOHS((unsigned short)
							((unsigned short *) pMsg->data)[8]);
					if ((FrameLength - IpFrameLength) == 0xe) {
						Csum1=le16_to_cpu(pRxd->TcpSums & 0xffff);
						Csum2=le16_to_cpu((pRxd->TcpSums >> 16) & 0xffff);
						if ((((Csum1 & 0xfffe) && (Csum2 & 0xfffe)) &&
							(pAC->GIni.GIChipId == CHIP_ID_GENESIS)) ||
							(pAC->ChipsetType)) {
							Result = SkCsGetReceiveInfo(pAC, &pMsg->data[14],
								Csum1, Csum2, PortIndex);
							if ((Result == SKCS_STATUS_IP_FRAGMENT) ||
							    (Result == SKCS_STATUS_IP_CSUM_OK)  ||
							    (Result == SKCS_STATUS_TCP_CSUM_OK) ||
							    (Result == SKCS_STATUS_UDP_CSUM_OK)) {
								pMsg->ip_summed = CHECKSUM_UNNECESSARY;
							} else if ((Result == SKCS_STATUS_TCP_CSUM_ERROR)    ||
							           (Result == SKCS_STATUS_UDP_CSUM_ERROR)    ||
							           (Result == SKCS_STATUS_IP_CSUM_ERROR_UDP) ||
							           (Result == SKCS_STATUS_IP_CSUM_ERROR_TCP) ||
							           (Result == SKCS_STATUS_IP_CSUM_ERROR)) {
								/* HW Checksum error */
								SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
								SK_DBGCAT_DRV_RX_PROGRESS,
								("skge: CRC error. Frame dropped!\n"));
								goto rx_failed;
							} else {
								pMsg->ip_summed = CHECKSUM_NONE;
							}
						}/* checksumControl calculation valid */
					} /* Frame length check */
				} /* IP frame */
			} /* pRxPort->UseRxCsum */
		} /* frame > SK_COPY_TRESHOLD */
		
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV,	1,("V"));
		RlmtNotifier = SK_RLMT_RX_PROTOCOL;
		SK_RLMT_PRE_LOOKAHEAD(pAC, PortIndex, FrameLength,
					IsBc, &Offset, &NumBytes);
		if (NumBytes != 0) {
			SK_RLMT_LOOKAHEAD(pAC,PortIndex,&pMsg->data[Offset],
						IsBc,IsMc,&RlmtNotifier);
		}
		if (RlmtNotifier == SK_RLMT_RX_PROTOCOL) {
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,	1,("W"));
			/* send up only frames from active port */
			if ((PortIndex == pAC->ActivePort)||(pAC->RlmtNets == 2)) {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV, 1,("U"));
#ifdef xDEBUG
				DumpMsg(pMsg, "Rx");
#endif
				SK_PNMI_CNT_RX_OCTETS_DELIVERED(pAC,FrameLength,PortIndex);
				pMsg->dev = pAC->dev[PortIndex];
				pMsg->protocol = eth_type_trans(pMsg,pAC->dev[PortIndex]);
				netif_rx(pMsg); /* frame for upper layer */
				pAC->dev[PortIndex]->last_rx = jiffies;
			} else {
				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,("D"));
				
				DEV_KFREE_SKB(pMsg); /* drop frame */
			}
		} else { /* packet for RLMT stack */
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
				SK_DBGCAT_DRV_RX_PROGRESS,("R"));
			pRlmtMbuf = SkDrvAllocRlmtMbuf(pAC,
				pAC->IoBase, FrameLength);
			if (pRlmtMbuf != NULL) {
				pRlmtMbuf->pNext = NULL;
				pRlmtMbuf->Length = FrameLength;
				pRlmtMbuf->PortIdx = PortIndex;
				EvPara.pParaPtr = pRlmtMbuf;
				memcpy((char*)(pRlmtMbuf->pData),
					   (char*)(pMsg->data),
					   FrameLength);

				/* SlowPathLock needed? */
				if (SlowPathLock == SK_TRUE) {
					KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
					SkEventQueue(pAC, SKGE_RLMT,
						SK_RLMT_PACKET_RECEIVED,
						EvPara);
					pAC->CheckQueue = SK_TRUE;
					KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
				} else {
					SkEventQueue(pAC, SKGE_RLMT,
						SK_RLMT_PACKET_RECEIVED,
						EvPara);
					pAC->CheckQueue = SK_TRUE;
				}

				SK_DBG_MSG(NULL, SK_DBGMOD_DRV,
					SK_DBGCAT_DRV_RX_PROGRESS,("Q"));
			}
			if ((pAC->dev[PortIndex]->flags & (IFF_PROMISC | IFF_ALLMULTI)) ||
			    (RlmtNotifier & SK_RLMT_RX_PROTOCOL)) {
				pMsg->dev = pAC->dev[PortIndex];
				pMsg->protocol = eth_type_trans(pMsg,pAC->dev[PortIndex]);
				netif_rx(pMsg);
				pAC->dev[PortIndex]->last_rx = jiffies;
			} else {				
				DEV_KFREE_SKB(pMsg);
			}
		} /* if packet for RLMT stack */
	} /* for ... scanning the RXD ring */

	/* RXD ring is empty -> fill and restart */
	FillRxRing(pAC, pRxPort);
	return;

rx_failed:
	/* remove error frame */
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ERROR,
		("Schrottdescriptor, length: 0x%x\n", FrameLength));

	/* release the DMA mapping */

	PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
	PhysAddr |= (SK_U64) pRxd->VDataLow;
/*	pci_unmap_page(pAC->PciDev,
			 PhysAddr,
			 pRxPort->RxBufSize - 2,
			 PCI_DMA_FROMDEVICE);
 */
	
	DEV_KFREE_SKB_IRQ(pRxd->pMBuf);
	pRxd->pMBuf = NULL;
	pRxPort->RxdRingFree++;
	pRxPort->pRxdRingHead = pRxd->pNextRxd;
	goto rx_start;

} /* ReceiveIrq */

/*****************************************************************************
 *
 * 	ClearRxRing - remove all buffers from the receive ring
 *
 * Description:
 *	This function removes all receive buffers from the ring.
 *	The receive BMU must be stopped before calling this function.
 *
 * Returns: N/A
 */
static void ClearRxRing(
SK_AC	*pAC,		/* pointer to adapter context */
RX_PORT	*pRxPort)	/* pointer to rx port struct */
{
RXD		*pRxd;	/* pointer to the current descriptor */
KIRQL	Irql;
SK_U64		PhysAddr;

	if (pRxPort->RxdRingFree == pAC->RxDescrPerRing) {
		return;
	}
	KeAcquireSpinLock(&pRxPort->RxDesRingLock, &Irql);
	pRxd = pRxPort->pRxdRingHead;
	do {
		if (pRxd->pMBuf != NULL) {

			PhysAddr = ((SK_U64) pRxd->VDataHigh) << (SK_U64)32;
			PhysAddr |= (SK_U64) pRxd->VDataLow;
/*
			pci_unmap_page(pAC->PciDev,
					 PhysAddr,
					 pRxPort->RxBufSize - 2,
					 PCI_DMA_FROMDEVICE);
 */			
			DEV_KFREE_SKB(pRxd->pMBuf);
			pRxd->pMBuf = NULL;
		}
		pRxd->RBControl &= BMU_OWN;
		pRxd = pRxd->pNextRxd;
		pRxPort->RxdRingFree++;
	} while (pRxd != pRxPort->pRxdRingTail);
	pRxPort->pRxdRingTail = pRxPort->pRxdRingHead;
	KeReleaseSpinLock(&pRxPort->RxDesRingLock, Irql);
} /* ClearRxRing */

/*****************************************************************************
 *
 *	ClearTxRing - remove all buffers from the transmit ring
 *
 * Description:
 *	This function removes all transmit buffers from the ring.
 *	The transmit BMU must be stopped before calling this function
 *	and transmitting at the upper level must be disabled.
 *	The BMU own bit of all descriptors is cleared, the rest is
 *	done by calling FreeTxDescriptors.
 *
 * Returns: N/A
 */
static void ClearTxRing(
SK_AC	*pAC,		/* pointer to adapter context */
TX_PORT	*pTxPort)	/* pointer to tx prt struct */
{
TXD		*pTxd;		/* pointer to the current descriptor */
int		i;
KIRQL	Irql;

	KeAcquireSpinLock(&pTxPort->TxDesRingLock, &Irql);
	pTxd = pTxPort->pTxdRingHead;
	for (i=0; i<pAC->TxDescrPerRing; i++) {
		pTxd->TBControl &= ~BMU_OWN;
		pTxd = pTxd->pNextTxd;
	}
	FreeTxDescriptors(pAC, pTxPort);
	KeReleaseSpinLock(&pTxPort->TxDesRingLock, Irql);
} /* ClearTxRing */

/*****************************************************************************
 *
 * 	SkGeSetMacAddr - Set the hardware MAC address
 *
 * Description:
 *	This function sets the MAC address used by the adapter.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */

#if 0
static int SkGeSetMacAddr(struct SK_NET_DEVICE *dev, void *p)
{

DEV_NET *pNet = (DEV_NET*) dev->priv;
SK_AC	*pAC = pNet->pAC;
int	Ret;

struct sockaddr	*addr = p;
KIRQL	Irql;
	
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeSetMacAddr starts now...\n"));

	memcpy(dev->dev_addr, addr->sa_data,dev->addr_len);
	
	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);

	if (pAC->RlmtNets == 2)
		Ret = SkAddrOverride(pAC, pAC->IoBase, pNet->NetNr,
			(SK_MAC_ADDR*)dev->dev_addr, SK_ADDR_VIRTUAL_ADDRESS);
	else
		Ret = SkAddrOverride(pAC, pAC->IoBase, pAC->ActivePort,
			(SK_MAC_ADDR*)dev->dev_addr, SK_ADDR_VIRTUAL_ADDRESS);
	
	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);

	if (Ret != SK_ADDR_OVERRIDE_SUCCESS)
		return -EBUSY;

	return 0;
} /* SkGeSetMacAddr */
#endif // #if 0

/*****************************************************************************
 *
 * 	SkGeSetRxMode - set receive mode
 *
 * Description:
 *	This function sets the receive mode of an adapter. The adapter
 *	supports promiscuous mode, allmulticast mode and a number of
 *	multicast addresses. If more multicast addresses the available
 *	are selected, a hash function in the hardware is used.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static void SkGeSetRxMode(struct SK_NET_DEVICE *dev)
{

DEV_NET		*pNet;
SK_AC		*pAC;

struct dev_mc_list	*pMcList;
int			i;
int			PortIdx;
KIRQL		Irql;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeSetRxMode starts now... "));

	pNet = (DEV_NET*) dev->priv;
	pAC = pNet->pAC;
	if (pAC->RlmtNets == 1)
		PortIdx = pAC->ActivePort;
	else
		PortIdx = pNet->NetNr;

	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
	if (dev->flags & IFF_PROMISC) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
			("PROMISCUOUS mode\n"));
		SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
			SK_PROM_MODE_LLC);
	} else if (dev->flags & IFF_ALLMULTI) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
			("ALLMULTI mode\n"));
		SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
			SK_PROM_MODE_ALL_MC);
	} else {
		SkAddrPromiscuousChange(pAC, pAC->IoBase, PortIdx,
			SK_PROM_MODE_NONE);
		SkAddrMcClear(pAC, pAC->IoBase, PortIdx, 0);

		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
			("Number of MC entries: %d ", dev->mc_count));
		
		pMcList = dev->mc_list;
		for (i=0; i<dev->mc_count; i++, pMcList = pMcList->next) {
			SkAddrMcAdd(pAC, pAC->IoBase, PortIdx,
				(SK_MAC_ADDR*)pMcList->dmi_addr, 0);
			SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_MCA,
				("%02x:%02x:%02x:%02x:%02x:%02x\n",
				pMcList->dmi_addr[0],
				pMcList->dmi_addr[1],
				pMcList->dmi_addr[2],
				pMcList->dmi_addr[3],
				pMcList->dmi_addr[4],
				pMcList->dmi_addr[5]));
		}
		SkAddrMcUpdate(pAC, pAC->IoBase, PortIdx);
	}
	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
	
	return;
} /* SkGeSetRxMode */


/*****************************************************************************
 *
 * 	SkSetMtuBufferSize - set the MTU buffer to another value
 *
 * Description:
 *	This function sets the new buffers and is called whenever the MTU 
 *      size is changed
 *
 * Returns:
 *	N/A
 */

static void SkSetMtuBufferSize(
SK_AC	*pAC,		/* pointer to adapter context */
int	PortNr,		/* Port number */
int	Mtu)		/* pointer to tx prt struct */
{
	pAC->RxPort[PortNr].RxBufSize = Mtu + 32;

	/* RxBufSize must be a multiple of 8 */
	while (pAC->RxPort[PortNr].RxBufSize % 8) {
		pAC->RxPort[PortNr].RxBufSize = 
			pAC->RxPort[PortNr].RxBufSize + 1;
	}

	if (Mtu > 1500) {
		pAC->GIni.GP[PortNr].PPortUsage = SK_JUMBO_LINK;
	} else {
		if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
			pAC->GIni.GP[PortNr].PPortUsage = SK_MUL_LINK;
		} else {
			pAC->GIni.GP[PortNr].PPortUsage = SK_RED_LINK;
		}
	}

	return;
}


/*****************************************************************************
 *
 * 	SkGeChangeMtu - set the MTU to another value
 *
 * Description:
 *	This function sets is called whenever the MTU size is changed
 *	(ifconfig mtu xxx dev ethX). If the MTU is bigger than standard
 *	ethernet MTU size, long frame support is activated.
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */
static int SkGeChangeMtu(struct SK_NET_DEVICE *dev, int NewMtu)
{
DEV_NET			*pNet;
SK_AC			*pAC;
KIRQL			Irql;
#ifdef CONFIG_SK98LIN_NAPI
int			WorkToDo = 1; // min(*budget, dev->quota);
int			WorkDone = 0;
#endif

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeChangeMtu starts now...\n"));

	pNet = (DEV_NET*) dev->priv;
	pAC  = pNet->pAC;

	/* MTU size outside the spec */
	if ((NewMtu < 68) || (NewMtu > SK_JUMBO_MTU)) {
		return -EINVAL;
	}

	/* MTU > 1500 on yukon ulra not allowed */
	if ((pAC->GIni.GIChipId == CHIP_ID_YUKON_EC_U) 
		&& (NewMtu > 1500)){
		return -EINVAL;
	}

	/* Diag access active */
	if (pAC->DiagModeActive == DIAG_ACTIVE) {
		if (pAC->DiagFlowCtrl == SK_FALSE) {
			return -1; /* still in use, deny any actions of MTU */
		} else {
			pAC->DiagFlowCtrl = SK_FALSE;
		}
	}

	dev->mtu = NewMtu;
	SkSetMtuBufferSize(pAC, pNet->PortNr, NewMtu);

	if(!netif_running(dev)) {
	/* Preset MTU size if device not ready/running */
		return 0;
	}

	/*  Prevent any reconfiguration while changing the MTU 
	    by disabling any interrupts */
	SK_OUT32(pAC->IoBase, B0_IMSK, 0);
	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);

	/* Notify RLMT that the port has to be stopped */
	netif_stop_queue(dev);
	SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_STOP,
				pNet->PortNr, -1, SK_TRUE);
	KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW].TxDesRingLock);


	/* Change RxFillLimit to 1 */
	if ((pAC->GIni.GIMacsFound == 2 ) && (pAC->RlmtNets == 2)) {
		pAC->RxPort[pNet->PortNr].RxFillLimit = 1;
	} else {
		pAC->RxPort[1 - pNet->PortNr].RxFillLimit = 1;
		pAC->RxPort[pNet->PortNr].RxFillLimit = pAC->RxDescrPerRing -
					(pAC->RxDescrPerRing / 4);
	}

	/* clear and reinit the rx rings here, because of new MTU size */
	if (CHIP_ID_YUKON_2(pAC)) {
		SkY2PortStop(pAC, pAC->IoBase, pNet->PortNr, SK_STOP_ALL, SK_SOFT_RST);
		SkY2AllocateRxBuffers(pAC, pAC->IoBase, pNet->PortNr);
		SkY2PortStart(pAC, pAC->IoBase, pNet->PortNr);
	} else {
//		SkGeStopPort(pAC, pAC->IoBase, pNet->PortNr, SK_STOP_ALL, SK_SOFT_RST);
#ifdef CONFIG_SK98LIN_NAPI
		WorkToDo = 1;
		ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE, &WorkDone, WorkToDo);
#else
		ReceiveIrq(pAC, &pAC->RxPort[pNet->PortNr], SK_TRUE);
#endif
		ClearRxRing(pAC, &pAC->RxPort[pNet->PortNr]);
		FillRxRing(pAC, &pAC->RxPort[pNet->PortNr]);

		/* Enable transmit descriptor polling */
		SkGePollTxD(pAC, pAC->IoBase, pNet->PortNr, SK_TRUE);
		FillRxRing(pAC, &pAC->RxPort[pNet->PortNr]);
	}

	netif_start_queue(pAC->dev[pNet->PortNr]);

	KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[pNet->PortNr][TX_PRIO_LOW].TxDesRingLock);


	/* Notify RLMT about the changing and restarting one (or more) ports */
	SkLocalEventQueue(pAC, SKGE_RLMT, SK_RLMT_START,
					pNet->PortNr, -1, SK_TRUE);

	/* Enable Interrupts again */
	SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
	SK_OUT32(pAC->IoBase, B0_HWE_IMSK, IRQ_HWE_MASK);

	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
	return 0;

}


/*****************************************************************************
 *
 * 	SkGeStats - return ethernet device statistics
 *
 * Description:
 *	This function return statistic data about the ethernet device
 *	to the operating system.
 *
 * Returns:
 *	pointer to the statistic structure.
 */
static struct net_device_stats *SkGeStats(struct SK_NET_DEVICE *dev)
{
	DEV_NET		*pNet = (DEV_NET*) dev->priv;
	SK_AC		*pAC = pNet->pAC;
	unsigned long	LateCollisions, ExcessiveCollisions, RxTooLong;
	KIRQL		Irql; /* for spin lock */
    SK_U32		MaxNumOidEntries, Oid, Len;
	char		Buf[8];
	struct {
		SK_U32         Oid;
		unsigned long *pVar;
	} Vars[] = {
		{ OID_SKGE_STAT_TX_LATE_COL,   &LateCollisions               },
		{ OID_SKGE_STAT_TX_EXCESS_COL, &ExcessiveCollisions          },
		{ OID_SKGE_STAT_RX_TOO_LONG,   &RxTooLong                    },
		{ OID_SKGE_STAT_RX,            &pAC->stats.rx_packets        },
		{ OID_SKGE_STAT_TX,            &pAC->stats.tx_packets        },
		{ OID_SKGE_STAT_RX_OCTETS,     &pAC->stats.rx_bytes          },
		{ OID_SKGE_STAT_TX_OCTETS,     &pAC->stats.tx_bytes          },
		{ OID_SKGE_RX_NO_BUF_CTS,      &pAC->stats.rx_dropped        },
		{ OID_SKGE_TX_NO_BUF_CTS,      &pAC->stats.tx_dropped        },
		{ OID_SKGE_STAT_RX_MULTICAST,  &pAC->stats.multicast         },
		{ OID_SKGE_STAT_RX_RUNT,       &pAC->stats.rx_length_errors  },
		{ OID_SKGE_STAT_RX_FCS,        &pAC->stats.rx_crc_errors     },
		{ OID_SKGE_STAT_RX_FRAMING,    &pAC->stats.rx_frame_errors   },
		{ OID_SKGE_STAT_RX_OVERFLOW,   &pAC->stats.rx_over_errors    },
		{ OID_SKGE_STAT_RX_MISSED,     &pAC->stats.rx_missed_errors  },
		{ OID_SKGE_STAT_TX_CARRIER,    &pAC->stats.tx_carrier_errors },
		{ OID_SKGE_STAT_TX_UNDERRUN,   &pAC->stats.tx_fifo_errors    },
	};
	
	if ((pAC->DiagModeActive == DIAG_NOTACTIVE) &&
	    (pAC->BoardLevel     == SK_INIT_RUN)) {
		memset(&pAC->stats, 0x00, sizeof(pAC->stats)); /* clean first */
		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);

    		MaxNumOidEntries = sizeof(Vars) / sizeof(Vars[0]);
    		for (Oid = 0; Oid < MaxNumOidEntries; Oid++) {
			if (SkPnmiGetVar(pAC,pAC->IoBase, Vars[Oid].Oid,
				&Buf, &Len, 1, pNet->NetNr) != SK_PNMI_ERR_OK) {
				memset(Buf, 0x00, sizeof(Buf));
			}
			*Vars[Oid].pVar = (unsigned long) (*((SK_U64 *) Buf));
		}
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);

		pAC->stats.collisions =	LateCollisions + ExcessiveCollisions;
		pAC->stats.tx_errors =	pAC->stats.tx_carrier_errors +
					pAC->stats.tx_fifo_errors;
		pAC->stats.rx_errors =	pAC->stats.rx_length_errors + 
					pAC->stats.rx_crc_errors +
					pAC->stats.rx_frame_errors + 
					pAC->stats.rx_over_errors +
					pAC->stats.rx_missed_errors;

		if (dev->mtu > 1500) {
			pAC->stats.rx_errors = pAC->stats.rx_errors - RxTooLong;
		}
	}

	return(&pAC->stats);
} /* SkGeStats */

/*****************************************************************************
 *
 * 	SkGeIoctl - IO-control function
 *
 * Description:
 *	This function is called if an ioctl is issued on the device.
 *	There are three subfunction for reading, writing and test-writing
 *	the private MIB data structure (usefull for SysKonnect-internal tools).
 *
 * Returns:
 *	0, if everything is ok
 *	!=0, on error
 */

#if 0
static int SkGeIoctl(
struct SK_NET_DEVICE *dev,  /* the device the IOCTL is to be performed on   */
struct ifreq         *rq,   /* additional request structure containing data */
int                   cmd)  /* requested IOCTL command number               */
{
	DEV_NET          *pNet = (DEV_NET*) dev->priv;
	SK_AC            *pAC  = pNet->pAC;
	struct pci_dev   *pdev = NULL;
	void             *pMemBuf;
	SK_GE_IOCTL       Ioctl;
	KIRQL			 Irql; /* for spin lock */
	unsigned int      Err = 0;
	unsigned int      Length = 0;
	int               HeaderLength = sizeof(SK_U32) + sizeof(SK_U32);
	int               Size = 0;
	int               Ret = 0;

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeIoctl starts now...\n"));

	if(copy_from_user(&Ioctl, rq->ifr_data, sizeof(SK_GE_IOCTL))) {
		return -EFAULT;
	}

	switch(cmd) {
	case SIOCETHTOOL:
		return SkEthIoctl(dev, rq);
	case SK_IOCTL_SETMIB:     /* FALL THRU */
	case SK_IOCTL_PRESETMIB:  /* FALL THRU (if capable!) */
		if (!capable(CAP_NET_ADMIN)) return -EPERM;
 	case SK_IOCTL_GETMIB:
		if(copy_from_user(&pAC->PnmiStruct, Ioctl.pData,
			Ioctl.Len<sizeof(pAC->PnmiStruct)?
			Ioctl.Len : sizeof(pAC->PnmiStruct))) {
			return -EFAULT;
		}
		Size = SkGeIocMib(pNet, Ioctl.Len, cmd);
		if(copy_to_user(Ioctl.pData, &pAC->PnmiStruct,
			Ioctl.Len<Size? Ioctl.Len : Size)) {
			return -EFAULT;
		}
		Ioctl.Len = Size;
		if(copy_to_user(rq->ifr_data, &Ioctl, sizeof(SK_GE_IOCTL))) {
			return -EFAULT;
		}
		break;
	case SK_IOCTL_GEN:
		if (Ioctl.Len < (sizeof(pAC->PnmiStruct) + HeaderLength)) {
			Length = Ioctl.Len;
		} else {
			Length = sizeof(pAC->PnmiStruct) + HeaderLength;
		}
		if (NULL == (pMemBuf = kmalloc(Length, GFP_KERNEL))) {
			return -ENOMEM;
		}
		KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
		if(copy_from_user(pMemBuf, Ioctl.pData, Length)) {
			Err = -EFAULT;
			goto fault_gen;
		}
		if ((Ret = SkPnmiGenIoctl(pAC, pAC->IoBase, pMemBuf, &Length, 0)) < 0) {
			Err = -EFAULT;
			goto fault_gen;
		}
		if(copy_to_user(Ioctl.pData, pMemBuf, Length) ) {
			Err = -EFAULT;
			goto fault_gen;
		}
		Ioctl.Len = Length;
		if(copy_to_user(rq->ifr_data, &Ioctl, sizeof(SK_GE_IOCTL))) {
			Err = -EFAULT;
			goto fault_gen;
		}
fault_gen:
		KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
		kfree(pMemBuf); /* cleanup everything */
		break;
	case SK_IOCTL_DIAG:
		if (!capable(CAP_NET_ADMIN)) return -EPERM;
		if (Ioctl.Len < (sizeof(pAC->PnmiStruct) + HeaderLength)) {
			Length = Ioctl.Len;
		} else {
			Length = sizeof(pAC->PnmiStruct) + HeaderLength;
		}
		if (NULL == (pMemBuf = kmalloc(Length, GFP_KERNEL))) {
			return -ENOMEM;
		}
		if(copy_from_user(pMemBuf, Ioctl.pData, Length)) {
			Err = -EFAULT;
			goto fault_diag;
		}
		pdev = pAC->PciDev;
		Length = 3 * sizeof(SK_U32);  /* Error, Bus and Device */
		/* 
		** While coding this new IOCTL interface, only a few lines of code
		** are to to be added. Therefore no dedicated function has been 
		** added. If more functionality is added, a separate function 
		** should be used...
		*/
		* ((SK_U32 *)pMemBuf) = 0;
		* ((SK_U32 *)pMemBuf + 1) = pdev->bus->number;
		* ((SK_U32 *)pMemBuf + 2) = ParseDeviceNbrFromSlotName(pdev->slot_name);
		if(copy_to_user(Ioctl.pData, pMemBuf, Length) ) {
			Err = -EFAULT;
			goto fault_diag;
		}
		Ioctl.Len = Length;
		if(copy_to_user(rq->ifr_data, &Ioctl, sizeof(SK_GE_IOCTL))) {
			Err = -EFAULT;
			goto fault_diag;
		}
fault_diag:
		kfree(pMemBuf); /* cleanup everything */
		break;
	default:
		Err = -EOPNOTSUPP;
	}

	return(Err);

} /* SkGeIoctl */

#endif // #if 0
/*****************************************************************************
 *
 * 	SkGeIocMib - handle a GetMib, SetMib- or PresetMib-ioctl message
 *
 * Description:
 *	This function reads/writes the MIB data using PNMI (Private Network
 *	Management Interface).
 *	The destination for the data must be provided with the
 *	ioctl call and is given to the driver in the form of
 *	a user space address.
 *	Copying from the user-provided data area into kernel messages
 *	and back is done by copy_from_user and copy_to_user calls in
 *	SkGeIoctl.
 *
 * Returns:
 *	returned size from PNMI call
 */

#if 0
static int SkGeIocMib(
DEV_NET		*pNet,	/* pointer to the adapter context */
unsigned int	Size,	/* length of ioctl data */
int		mode)	/* flag for set/preset */
{
	SK_AC		*pAC = pNet->pAC;
	KIRQL		Irql;  /* for spin lock */

	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("SkGeIocMib starts now...\n"));

	/* access MIB */
	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
	switch(mode) {
	case SK_IOCTL_GETMIB:
		SkPnmiGetStruct(pAC, pAC->IoBase, &pAC->PnmiStruct, &Size,
			pNet->NetNr);
		break;
	case SK_IOCTL_PRESETMIB:
		SkPnmiPreSetStruct(pAC, pAC->IoBase, &pAC->PnmiStruct, &Size,
			pNet->NetNr);
		break;
	case SK_IOCTL_SETMIB:
		SkPnmiSetStruct(pAC, pAC->IoBase, &pAC->PnmiStruct, &Size,
			pNet->NetNr);
		break;
	default:
		break;
	}
	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ENTRY,
		("MIB data access succeeded\n"));
	return (Size);
} /* SkGeIocMib */
#endif // #if 0

/*****************************************************************************
 *
 * 	GetConfiguration - read configuration information
 *
 * Description:
 *	This function reads per-adapter configuration information from
 *	the options provided on the command line.
 *
 * Returns:
 *	none
 */
static void GetConfiguration(
SK_AC	*pAC)	/* pointer to the adapter context structure */
{
SK_I32	Port;		/* preferred port */
SK_BOOL	AutoSet;
SK_BOOL DupSet;
int	LinkSpeed		= SK_LSPEED_AUTO;	/* Link speed */
int	AutoNeg			= 1;			/* autoneg off (0) or on (1) */
int	DuplexCap		= 0;			/* 0=both,1=full,2=half */
int	FlowCtrl		= SK_FLOW_MODE_SYM_OR_REM;	/* FlowControl  */
int	MSMode			= SK_MS_MODE_AUTO;	/* master/slave mode    */
int	IrqModMaskOffset	= 6;			/* all ints moderated=default */

SK_BOOL IsConTypeDefined	= SK_TRUE;
SK_BOOL IsLinkSpeedDefined	= SK_TRUE;
SK_BOOL IsFlowCtrlDefined	= SK_TRUE;
SK_BOOL IsRoleDefined		= SK_TRUE;
SK_BOOL IsModeDefined		= SK_TRUE;
/*
 *	The two parameters AutoNeg. and DuplexCap. map to one configuration
 *	parameter. The mapping is described by this table:
 *	DuplexCap ->	|	both	|	full	|	half	|
 *	AutoNeg		|		|		|		|
 *	-----------------------------------------------------------------
 *	Off		|    illegal	|	Full	|	Half	|
 *	-----------------------------------------------------------------
 *	On		|   AutoBoth	|   AutoFull	|   AutoHalf	|
 *	-----------------------------------------------------------------
 *	Sense		|   AutoSense	|   AutoSense	|   AutoSense	|
 */
int	Capabilities[3][3] =
		{ {                -1, SK_LMODE_FULL     , SK_LMODE_HALF     },
		  {SK_LMODE_AUTOBOTH , SK_LMODE_AUTOFULL , SK_LMODE_AUTOHALF },
		  {SK_LMODE_AUTOSENSE, SK_LMODE_AUTOSENSE, SK_LMODE_AUTOSENSE} };

SK_U32	IrqModMask[7][2] =
		{ { IRQ_MASK_RX_ONLY , Y2_DRIVER_IRQS  },
		  { IRQ_MASK_TX_ONLY , Y2_DRIVER_IRQS  },
		  { IRQ_MASK_SP_ONLY , Y2_SPECIAL_IRQS },
		  { IRQ_MASK_SP_RX   , Y2_IRQ_MASK     },
		  { IRQ_MASK_TX_RX   , Y2_DRIVER_IRQS  },
		  { IRQ_MASK_SP_TX   , Y2_IRQ_MASK     },
		  { IRQ_MASK_RX_TX_SP, Y2_IRQ_MASK     } };

#define DC_BOTH	0
#define DC_FULL 1
#define DC_HALF 2
#define AN_OFF	0
#define AN_ON	1
#define AN_SENS	2
#define M_CurrPort pAC->GIni.GP[Port]


	/*
	** Set the default values first for both ports!
	*/
	for (Port = 0; Port < SK_MAX_MACS; Port++) {
		M_CurrPort.PLinkModeConf = (UCHAR) Capabilities[AN_ON][DC_BOTH];
		M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_SYM_OR_REM;
		M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
		M_CurrPort.PLinkSpeed    = SK_LSPEED_AUTO;
	}

	/*
	** Check merged parameter ConType. If it has not been used,
	** verify any other parameter (e.g. AutoNeg) and use default values. 
	**
	** Stating both ConType and other lowlevel link parameters is also
	** possible. If this is the case, the passed ConType-parameter is 
	** overwritten by the lowlevel link parameter.
	**
	** The following settings are used for a merged ConType-parameter:
	**
	** ConType   DupCap   AutoNeg   FlowCtrl      Role      Speed
	** -------   ------   -------   --------   ----------   -----
	**  Auto      Both      On      SymOrRem      Auto       Auto
	**  100FD     Full      Off       None      <ignored>    100
	**  100HD     Half      Off       None      <ignored>    100
	**  10FD      Full      Off       None      <ignored>    10
	**  10HD      Half      Off       None      <ignored>    10
	** 
	** This ConType parameter is used for all ports of the adapter!
	*/
	if ( (ConType != NULL)                && 
	     (pAC->Index < SK_MAX_CARD_PARAM) &&
	     (ConType[pAC->Index] != NULL) ) {

			/* Check chipset family */
			if ((!pAC->ChipsetType) && 
				(strcmp(ConType[pAC->Index],"Auto")!=0) &&
				(strcmp(ConType[pAC->Index],"")!=0)) {
				/* Set the speed parameter back */
					NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" " 
							"for ConType."
							" Using Auto.\n", 
							ConType[pAC->Index]));

					sprintf(ConType[pAC->Index], "Auto");	
			}

				if (strcmp(ConType[pAC->Index],"")==0) {
			IsConTypeDefined = SK_FALSE; /* No ConType defined */
				} else if (strcmp(ConType[pAC->Index],"Auto")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = (UCHAR) Capabilities[AN_ON][DC_BOTH];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_SYM_OR_REM;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_AUTO;
		    }
		} else if (strcmp(ConType[pAC->Index],"100FD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = (UCHAR) Capabilities[AN_OFF][DC_FULL];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_100MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"100HD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = (UCHAR) Capabilities[AN_OFF][DC_HALF];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_100MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"10FD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = (UCHAR) Capabilities[AN_OFF][DC_FULL];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_10MBPS;
		    }
		} else if (strcmp(ConType[pAC->Index],"10HD")==0) {
		    for (Port = 0; Port < SK_MAX_MACS; Port++) {
			M_CurrPort.PLinkModeConf = (UCHAR) Capabilities[AN_OFF][DC_HALF];
			M_CurrPort.PFlowCtrlMode = SK_FLOW_MODE_NONE;
			M_CurrPort.PMSMode       = SK_MS_MODE_AUTO;
			M_CurrPort.PLinkSpeed    = SK_LSPEED_10MBPS;
		    }
		} else { 
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for ConType\n", 
			ConType[pAC->Index]));
		    IsConTypeDefined = SK_FALSE; /* Wrong ConType defined */
		}
	} else {
	    IsConTypeDefined = SK_FALSE; /* No ConType defined */
	}

	/*
	** Parse any parameter settings for port A:
	** a) any LinkSpeed stated?
	*/
	if (Speed_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Speed_A[pAC->Index] != NULL) {
		if (strcmp(Speed_A[pAC->Index],"")==0) {
		    IsLinkSpeedDefined = SK_FALSE;
		} else if (strcmp(Speed_A[pAC->Index],"Auto")==0) {
		    LinkSpeed = SK_LSPEED_AUTO;
		} else if (strcmp(Speed_A[pAC->Index],"10")==0) {
		    LinkSpeed = SK_LSPEED_10MBPS;
		} else if (strcmp(Speed_A[pAC->Index],"100")==0) {
		    LinkSpeed = SK_LSPEED_100MBPS;
		} else if (strcmp(Speed_A[pAC->Index],"1000")==0) {
		    if ((pAC->PciDev->vendor == 0x11ab ) &&
		    	(pAC->PciDev->device == 0x4350)) {
				LinkSpeed = SK_LSPEED_100MBPS;
				NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for Speed_A.\n"
					"Gigabit speed not possible with this chip revision!",
					Speed_A[pAC->Index]));
			} else {
				LinkSpeed = SK_LSPEED_1000MBPS;
		    }
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for Speed_A\n",
			Speed_A[pAC->Index]));
		    IsLinkSpeedDefined = SK_FALSE;
		}
	} else {
		if ((pAC->PciDev->vendor == 0x11ab ) && 
			(pAC->PciDev->device == 0x4350)) {
			/* Gigabit speed not supported
			 * Swith to speed 100
			 */
			LinkSpeed = SK_LSPEED_100MBPS;
		} else {
			IsLinkSpeedDefined = SK_FALSE;
		}
	}

	/* 
	** Check speed parameter: 
	**    Only copper type adapter and GE V2 cards 
	*/
	if (((!pAC->ChipsetType) || (pAC->GIni.GICopperType != SK_TRUE)) &&
		((LinkSpeed != SK_LSPEED_AUTO) &&
		(LinkSpeed != SK_LSPEED_1000MBPS))) {
		NbDebugPrint(0, ("sk98lin: Illegal value for Speed_A. "
			"Not a copper card or GE V2 card\n    Using "
			"speed 1000\n"));
		LinkSpeed = SK_LSPEED_1000MBPS;
	}
	
	/*	
	** Decide whether to set new config value if somethig valid has
	** been received.
	*/
	if (IsLinkSpeedDefined) {
		pAC->GIni.GP[0].PLinkSpeed = (UCHAR) LinkSpeed;
	} 

	/* 
	** b) Any Autonegotiation and DuplexCapabilities set?
	**    Please note that both belong together...
	*/
	AutoNeg = AN_ON; /* tschilling: Default: Autonegotiation on! */
	AutoSet = SK_FALSE;
	if (AutoNeg_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		AutoNeg_A[pAC->Index] != NULL) {
		AutoSet = SK_TRUE;
		if (strcmp(AutoNeg_A[pAC->Index],"")==0) {
		    AutoSet = SK_FALSE;
		} else if (strcmp(AutoNeg_A[pAC->Index],"On")==0) {
		    AutoNeg = AN_ON;
		} else if (strcmp(AutoNeg_A[pAC->Index],"Off")==0) {
		    AutoNeg = AN_OFF;
		} else if (strcmp(AutoNeg_A[pAC->Index],"Sense")==0) {
		    AutoNeg = AN_SENS;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for AutoNeg_A\n",
			AutoNeg_A[pAC->Index]));
		}
	}

	DuplexCap = DC_BOTH;
	DupSet    = SK_FALSE;
	if (DupCap_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		DupCap_A[pAC->Index] != NULL) {
		DupSet = SK_TRUE;
		if (strcmp(DupCap_A[pAC->Index],"")==0) {
		    DupSet = SK_FALSE;
		} else if (strcmp(DupCap_A[pAC->Index],"Both")==0) {
		    DuplexCap = DC_BOTH;
		} else if (strcmp(DupCap_A[pAC->Index],"Full")==0) {
		    DuplexCap = DC_FULL;
		} else if (strcmp(DupCap_A[pAC->Index],"Half")==0) {
		    DuplexCap = DC_HALF;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for DupCap_A\n",
			DupCap_A[pAC->Index]));
		}
	}

	/* 
	** Check for illegal combinations 
	*/
	if ((LinkSpeed == SK_LSPEED_1000MBPS) &&
		((DuplexCap == SK_LMODE_STAT_AUTOHALF) ||
		(DuplexCap == SK_LMODE_STAT_HALF)) &&
		(pAC->ChipsetType)) {
		    NbDebugPrint(0, ("sk98lin: Half Duplex not possible with Gigabit speed!\n"
					"    Using Full Duplex.\n"));
				DuplexCap = DC_FULL;
	}

	if ( AutoSet && AutoNeg==AN_SENS && DupSet) {
		NbDebugPrint(0, ("sk98lin, Port A: DuplexCapabilities"
			" ignored using Sense mode\n"));
	}

	if (AutoSet && AutoNeg==AN_OFF && DupSet && DuplexCap==DC_BOTH){
		NbDebugPrint(0, ("sk98lin: Port A: Illegal combination"
			" of values AutoNeg. and DuplexCap.\n    Using "
			"Full Duplex\n"));
		DuplexCap = DC_FULL;
	}

	if (AutoSet && AutoNeg==AN_OFF && !DupSet) {
		DuplexCap = DC_FULL;
	}
	
	if (!AutoSet && DupSet) {
		AutoNeg = AN_ON;
	}
	
	/* 
	** set the desired mode 
	*/
	if (AutoSet || DupSet) {
	    pAC->GIni.GP[0].PLinkModeConf = (UCHAR) Capabilities[AutoNeg][DuplexCap];
	}
	
	/* 
	** c) Any Flowcontrol-parameter set?
	*/
	if (FlowCtrl_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		FlowCtrl_A[pAC->Index] != NULL) {
		if (strcmp(FlowCtrl_A[pAC->Index],"") == 0) {
		    IsFlowCtrlDefined = SK_FALSE;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"SymOrRem") == 0) {
		    FlowCtrl = SK_FLOW_MODE_SYM_OR_REM;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"Sym")==0) {
		    FlowCtrl = SK_FLOW_MODE_SYMMETRIC;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"LocSend")==0) {
		    FlowCtrl = SK_FLOW_MODE_LOC_SEND;
		} else if (strcmp(FlowCtrl_A[pAC->Index],"None")==0) {
		    FlowCtrl = SK_FLOW_MODE_NONE;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for FlowCtrl_A\n",
			FlowCtrl_A[pAC->Index]));
		    IsFlowCtrlDefined = SK_FALSE;
		}
	} else {
	   IsFlowCtrlDefined = SK_FALSE;
	}

	if (IsFlowCtrlDefined) {
	    if ((AutoNeg == AN_OFF) && (FlowCtrl != SK_FLOW_MODE_NONE)) {
		NbDebugPrint(0, ("sk98lin: Port A: FlowControl"
			" impossible without AutoNegotiation,"
			" disabled\n"));
		FlowCtrl = SK_FLOW_MODE_NONE;
	    }
	    pAC->GIni.GP[0].PFlowCtrlMode = (UCHAR) FlowCtrl;
	}

	/*
	** d) What is with the RoleParameter?
	*/
	if (Role_A != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Role_A[pAC->Index] != NULL) {
		if (strcmp(Role_A[pAC->Index],"")==0) {
		   IsRoleDefined = SK_FALSE;
		} else if (strcmp(Role_A[pAC->Index],"Auto")==0) {
		    MSMode = SK_MS_MODE_AUTO;
		} else if (strcmp(Role_A[pAC->Index],"Master")==0) {
		    MSMode = SK_MS_MODE_MASTER;
		} else if (strcmp(Role_A[pAC->Index],"Slave")==0) {
		    MSMode = SK_MS_MODE_SLAVE;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for Role_A\n",
			Role_A[pAC->Index]));
		    IsRoleDefined = SK_FALSE;
		}
	} else {
	   IsRoleDefined = SK_FALSE;
	}

	if (IsRoleDefined == SK_TRUE) {
	    pAC->GIni.GP[0].PMSMode = (UCHAR) MSMode;
	}
	

	
	/* 
	** Parse any parameter settings for port B:
	** a) any LinkSpeed stated?
	*/
	IsConTypeDefined   = SK_TRUE;
	IsLinkSpeedDefined = SK_TRUE;
	IsFlowCtrlDefined  = SK_TRUE;
	IsModeDefined      = SK_TRUE;

	if (Speed_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Speed_B[pAC->Index] != NULL) {
		if (strcmp(Speed_B[pAC->Index],"")==0) {
		    IsLinkSpeedDefined = SK_FALSE;
		} else if (strcmp(Speed_B[pAC->Index],"Auto")==0) {
		    LinkSpeed = SK_LSPEED_AUTO;
		} else if (strcmp(Speed_B[pAC->Index],"10")==0) {
		    LinkSpeed = SK_LSPEED_10MBPS;
		} else if (strcmp(Speed_B[pAC->Index],"100")==0) {
		    LinkSpeed = SK_LSPEED_100MBPS;
		} else if (strcmp(Speed_B[pAC->Index],"1000")==0) {
		    LinkSpeed = SK_LSPEED_1000MBPS;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for Speed_B\n",
			Speed_B[pAC->Index]));
		    IsLinkSpeedDefined = SK_FALSE;
		}
	} else {
	    IsLinkSpeedDefined = SK_FALSE;
	}

	/* 
	** Check speed parameter:
	**    Only copper type adapter and GE V2 cards 
	*/
	if (((!pAC->ChipsetType) || (pAC->GIni.GICopperType != SK_TRUE)) &&
		((LinkSpeed != SK_LSPEED_AUTO) &&
		(LinkSpeed != SK_LSPEED_1000MBPS))) {
		NbDebugPrint(0, ("sk98lin: Illegal value for Speed_B. "
			"Not a copper card or GE V2 card\n    Using "
			"speed 1000\n"));
		LinkSpeed = SK_LSPEED_1000MBPS;
	}

	/*      
	** Decide whether to set new config value if somethig valid has
	** been received.
	*/
	if (IsLinkSpeedDefined) {
	    pAC->GIni.GP[1].PLinkSpeed = (UCHAR) LinkSpeed;
	}

	/* 
	** b) Any Autonegotiation and DuplexCapabilities set?
	**    Please note that both belong together...
	*/
	AutoNeg = AN_SENS; /* default: do auto Sense */
	AutoSet = SK_FALSE;
	if (AutoNeg_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		AutoNeg_B[pAC->Index] != NULL) {
		AutoSet = SK_TRUE;
		if (strcmp(AutoNeg_B[pAC->Index],"")==0) {
		    AutoSet = SK_FALSE;
		} else if (strcmp(AutoNeg_B[pAC->Index],"On")==0) {
		    AutoNeg = AN_ON;
		} else if (strcmp(AutoNeg_B[pAC->Index],"Off")==0) {
		    AutoNeg = AN_OFF;
		} else if (strcmp(AutoNeg_B[pAC->Index],"Sense")==0) {
		    AutoNeg = AN_SENS;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for AutoNeg_B\n",
			AutoNeg_B[pAC->Index]));
		}
	}

	DuplexCap = DC_BOTH;
	DupSet    = SK_FALSE;
	if (DupCap_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		DupCap_B[pAC->Index] != NULL) {
		DupSet = SK_TRUE;
		if (strcmp(DupCap_B[pAC->Index],"")==0) {
		    DupSet = SK_FALSE;
		} else if (strcmp(DupCap_B[pAC->Index],"Both")==0) {
		    DuplexCap = DC_BOTH;
		} else if (strcmp(DupCap_B[pAC->Index],"Full")==0) {
		    DuplexCap = DC_FULL;
		} else if (strcmp(DupCap_B[pAC->Index],"Half")==0) {
		    DuplexCap = DC_HALF;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for DupCap_B\n",
			DupCap_B[pAC->Index]));
		}
	}

	
	/* 
	** Check for illegal combinations 
	*/
	if ((LinkSpeed == SK_LSPEED_1000MBPS) &&
		((DuplexCap == SK_LMODE_STAT_AUTOHALF) ||
		(DuplexCap == SK_LMODE_STAT_HALF)) &&
		(pAC->ChipsetType)) {
		    NbDebugPrint(0, ("sk98lin: Half Duplex not possible with Gigabit speed!\n"
					"    Using Full Duplex.\n"));
				DuplexCap = DC_FULL;
	}

	if (AutoSet && AutoNeg==AN_SENS && DupSet) {
		NbDebugPrint(0, ("sk98lin, Port B: DuplexCapabilities"
			" ignored using Sense mode\n"));
	}

	if (AutoSet && AutoNeg==AN_OFF && DupSet && DuplexCap==DC_BOTH){
		NbDebugPrint(0, ("sk98lin: Port B: Illegal combination"
			" of values AutoNeg. and DuplexCap.\n    Using "
			"Full Duplex\n"));
		DuplexCap = DC_FULL;
	}

	if (AutoSet && AutoNeg==AN_OFF && !DupSet) {
		DuplexCap = DC_FULL;
	}
	
	if (!AutoSet && DupSet) {
		AutoNeg = AN_ON;
	}

	/* 
	** set the desired mode 
	*/
	if (AutoSet || DupSet) {
	    pAC->GIni.GP[1].PLinkModeConf = (UCHAR) Capabilities[AutoNeg][DuplexCap];
	}

	/*
	** c) Any FlowCtrl parameter set?
	*/
	if (FlowCtrl_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		FlowCtrl_B[pAC->Index] != NULL) {
		if (strcmp(FlowCtrl_B[pAC->Index],"") == 0) {
		    IsFlowCtrlDefined = SK_FALSE;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"SymOrRem") == 0) {
		    FlowCtrl = SK_FLOW_MODE_SYM_OR_REM;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"Sym")==0) {
		    FlowCtrl = SK_FLOW_MODE_SYMMETRIC;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"LocSend")==0) {
		    FlowCtrl = SK_FLOW_MODE_LOC_SEND;
		} else if (strcmp(FlowCtrl_B[pAC->Index],"None")==0) {
		    FlowCtrl = SK_FLOW_MODE_NONE;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for FlowCtrl_B\n",
			FlowCtrl_B[pAC->Index]));
		    IsFlowCtrlDefined = SK_FALSE;
		}
	} else {
		IsFlowCtrlDefined = SK_FALSE;
	}

	if (IsFlowCtrlDefined) {
	    if ((AutoNeg == AN_OFF) && (FlowCtrl != SK_FLOW_MODE_NONE)) {
		NbDebugPrint(0, ("sk98lin: Port B: FlowControl"
			" impossible without AutoNegotiation,"
			" disabled\n"));
		FlowCtrl = SK_FLOW_MODE_NONE;
	    }
	    pAC->GIni.GP[1].PFlowCtrlMode = (UCHAR) FlowCtrl;
	}

	/*
	** d) What is the RoleParameter?
	*/
	if (Role_B != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		Role_B[pAC->Index] != NULL) {
		if (strcmp(Role_B[pAC->Index],"")==0) {
		    IsRoleDefined = SK_FALSE;
		} else if (strcmp(Role_B[pAC->Index],"Auto")==0) {
		    MSMode = SK_MS_MODE_AUTO;
		} else if (strcmp(Role_B[pAC->Index],"Master")==0) {
		    MSMode = SK_MS_MODE_MASTER;
		} else if (strcmp(Role_B[pAC->Index],"Slave")==0) {
		    MSMode = SK_MS_MODE_SLAVE;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for Role_B\n",
			Role_B[pAC->Index]));
		    IsRoleDefined = SK_FALSE;
		}
	} else {
	    IsRoleDefined = SK_FALSE;
	}

	if (IsRoleDefined) {
	    pAC->GIni.GP[1].PMSMode = (UCHAR) MSMode;
	}
	
	/*
	** Evaluate settings for both ports
	*/
	pAC->ActivePort = 0;
	if (PrefPort != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		PrefPort[pAC->Index] != NULL) {
		if (strcmp(PrefPort[pAC->Index],"") == 0) { /* Auto */
			pAC->ActivePort             =  0;
			pAC->Rlmt.Net[0].Preference = -1; /* auto */
			pAC->Rlmt.Net[0].PrefPort   =  0;
		} else if (strcmp(PrefPort[pAC->Index],"A") == 0) {
			/*
			** do not set ActivePort here, thus a port
			** switch is issued after net up.
			*/
			Port                        = 0;
			pAC->Rlmt.Net[0].Preference = Port;
			pAC->Rlmt.Net[0].PrefPort   = Port;
		} else if (strcmp(PrefPort[pAC->Index],"B") == 0) {
			/*
			** do not set ActivePort here, thus a port
			** switch is issued after net up.
			*/
			if (pAC->GIni.GIMacsFound == 1) {
				NbDebugPrint(0, ("sk98lin: Illegal value \"B\" for PrefPort.\n"
					"      Port B not available on single port adapters.\n"));

				pAC->ActivePort             =  0;
				pAC->Rlmt.Net[0].Preference = -1; /* auto */
				pAC->Rlmt.Net[0].PrefPort   =  0;
			} else {
				Port                        = 1;
				pAC->Rlmt.Net[0].Preference = Port;
				pAC->Rlmt.Net[0].PrefPort   = Port;
			}
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for PrefPort\n",
			PrefPort[pAC->Index]));
		}
	}

	pAC->RlmtNets = 1;
	pAC->RlmtMode = 0;

	if (RlmtMode != NULL && pAC->Index<SK_MAX_CARD_PARAM &&
		RlmtMode[pAC->Index] != NULL) {
		if (strcmp(RlmtMode[pAC->Index], "") == 0) {
			if (pAC->GIni.GIMacsFound == 2) {
				pAC->RlmtMode = SK_RLMT_CHECK_LINK;
				pAC->RlmtNets = 2;
			}
		} else if (strcmp(RlmtMode[pAC->Index], "CheckLinkState") == 0) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK;
		} else if (strcmp(RlmtMode[pAC->Index], "CheckLocalPort") == 0) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK |
					SK_RLMT_CHECK_LOC_LINK;
		} else if (strcmp(RlmtMode[pAC->Index], "CheckSeg") == 0) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK     |
					SK_RLMT_CHECK_LOC_LINK |
					SK_RLMT_CHECK_SEG;
		} else if ((strcmp(RlmtMode[pAC->Index], "DualNet") == 0) &&
			(pAC->GIni.GIMacsFound == 2)) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK;
			pAC->RlmtNets = 2;
		} else {
		    NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for"
			" RlmtMode, using default\n", 
			RlmtMode[pAC->Index]));
			pAC->RlmtMode = 0;
		}
	} else {
		if (pAC->GIni.GIMacsFound == 2) {
			pAC->RlmtMode = SK_RLMT_CHECK_LINK;
			pAC->RlmtNets = 2;
		}
	}

#ifdef SK_YUKON2
	/*
	** use dualnet config per default
	*
	pAC->RlmtMode = SK_RLMT_CHECK_LINK;
	pAC->RlmtNets = 2;
	*/
#endif


	/*
	** Check the LowLatance parameters
	*/
	pAC->LowLatency = SK_FALSE;
	if (LowLatency[pAC->Index] != NULL) {
		if (strcmp(LowLatency[pAC->Index], "On") == 0) {
			pAC->LowLatency = SK_TRUE;
		}
	}


	/*
	** Check the interrupt moderation parameters
	*/
	pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_NONE;
	if (Moderation[pAC->Index] != NULL) {
		if (strcmp(Moderation[pAC->Index], "") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_NONE;
		} else if (strcmp(Moderation[pAC->Index], "Static") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_STATIC;
		} else if (strcmp(Moderation[pAC->Index], "Dynamic") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_DYNAMIC;
		} else if (strcmp(Moderation[pAC->Index], "None") == 0) {
			pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_NONE;
		} else {
	   		NbDebugPrint(0, ("sk98lin: Illegal value \"%s\" for Moderation.\n"
				"      Disable interrupt moderation.\n",
				Moderation[pAC->Index]));
		}
	} else {
/* Set interrupt moderation if wished */
#ifdef CONFIG_SK98LIN_STATINT
		pAC->DynIrqModInfo.IntModTypeSelect = C_INT_MOD_STATIC;
#endif
	}

	if (ModerationMask[pAC->Index] != NULL) {
		if (strcmp(ModerationMask[pAC->Index], "Rx") == 0) {
			IrqModMaskOffset = 0;
		} else if (strcmp(ModerationMask[pAC->Index], "Tx") == 0) {
			IrqModMaskOffset = 1;
		} else if (strcmp(ModerationMask[pAC->Index], "Sp") == 0) {
			IrqModMaskOffset = 2;
		} else if (strcmp(ModerationMask[pAC->Index], "RxSp") == 0) {
			IrqModMaskOffset = 3;
		} else if (strcmp(ModerationMask[pAC->Index], "SpRx") == 0) {
			IrqModMaskOffset = 3;
		} else if (strcmp(ModerationMask[pAC->Index], "RxTx") == 0) {
			IrqModMaskOffset = 4;
		} else if (strcmp(ModerationMask[pAC->Index], "TxRx") == 0) {
			IrqModMaskOffset = 4;
		} else if (strcmp(ModerationMask[pAC->Index], "TxSp") == 0) {
			IrqModMaskOffset = 5;
		} else if (strcmp(ModerationMask[pAC->Index], "SpTx") == 0) {
			IrqModMaskOffset = 5;
		} else { /* some rubbish stated */
			// IrqModMaskOffset = 6; ->has been initialized
			// already at the begin of this function...
		}
	}
	if (!CHIP_ID_YUKON_2(pAC)) {
		pAC->DynIrqModInfo.MaskIrqModeration = IrqModMask[IrqModMaskOffset][0];
	} else {
		pAC->DynIrqModInfo.MaskIrqModeration = IrqModMask[IrqModMaskOffset][1];
	}

	if (!CHIP_ID_YUKON_2(pAC)) {
		pAC->DynIrqModInfo.MaxModIntsPerSec = C_INTS_PER_SEC_DEFAULT;
	} else {
		pAC->DynIrqModInfo.MaxModIntsPerSec = C_Y2_INTS_PER_SEC_DEFAULT;
	}
	if (IntsPerSec[pAC->Index] != 0) {
		if ((IntsPerSec[pAC->Index]< C_INT_MOD_IPS_LOWER_RANGE) || 
			(IntsPerSec[pAC->Index] > C_INT_MOD_IPS_UPPER_RANGE)) {
	   		NbDebugPrint(0, ("sk98lin: Illegal value \"%d\" for IntsPerSec. (Range: %d - %d)\n"
				"      Using default value of %i.\n", 
				IntsPerSec[pAC->Index],
				C_INT_MOD_IPS_LOWER_RANGE,
				C_INT_MOD_IPS_UPPER_RANGE,
				pAC->DynIrqModInfo.MaxModIntsPerSec));
		} else {
			pAC->DynIrqModInfo.MaxModIntsPerSec = IntsPerSec[pAC->Index];
		}
	} 

	/*
	** Evaluate upper and lower moderation threshold
	*/
	pAC->DynIrqModInfo.MaxModIntsPerSecUpperLimit =
		pAC->DynIrqModInfo.MaxModIntsPerSec +
		(pAC->DynIrqModInfo.MaxModIntsPerSec / 5);

	pAC->DynIrqModInfo.MaxModIntsPerSecLowerLimit =
		pAC->DynIrqModInfo.MaxModIntsPerSec -
		(pAC->DynIrqModInfo.MaxModIntsPerSec / 5);

	pAC->DynIrqModInfo.DynIrqModSampleInterval = 
		SK_DRV_MODERATION_TIMER_LENGTH;

} /* GetConfiguration */


/*****************************************************************************
 *
 * 	ProductStr - return a adapter identification string from vpd
 *
 * Description:
 *	This function reads the product name string from the vpd area
 *	and puts it the field pAC->DeviceString.
 *
 * Returns: N/A
 */
static void ProductStr(SK_AC *pAC)
{
	char Default[] = "Generic Marvell Yukon chipset Ethernet device";
	char Key[] = VPD_NAME; /* VPD productname key */
	int StrLen = 80;       /* stringlen           */
	KIRQL Irql;
	int ReturnCode;

	KeAcquireSpinLock(&pAC->SlowPathLock, &Irql);
	if (ReturnCode = VpdRead(pAC, pAC->IoBase, Key, pAC->DeviceStr, &StrLen)) {
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_ERROR,
			("Error reading VPD data: %d\n", ReturnCode));
		strcpy(pAC->DeviceStr, Default);
	}
	KeReleaseSpinLock(&pAC->SlowPathLock, Irql);
} /* ProductStr */

/****************************************************************************/
/* functions for common modules *********************************************/
/****************************************************************************/


/*****************************************************************************
 *
 *	SkDrvAllocRlmtMbuf - allocate an RLMT mbuf
 *
 * Description:
 *	This routine returns an RLMT mbuf or NULL. The RLMT Mbuf structure
 *	is embedded into a socket buff data area.
 *
 * Context:
 *	runtime
 *
 * Returns:
 *	NULL or pointer to Mbuf.
 */
SK_MBUF *SkDrvAllocRlmtMbuf(
SK_AC		*pAC,		/* pointer to adapter context */
SK_IOC		IoC,		/* the IO-context */
unsigned	BufferSize)	/* size of the requested buffer */
{
SK_MBUF		*pRlmtMbuf;	/* pointer to a new rlmt-mbuf structure */
struct sk_buff	*pMsgBlock;	/* pointer to a new message block */

	NbDebugPrint(0, ("SkDrvAllocRlmtMbuf\n"));
	pMsgBlock = alloc_skb(BufferSize + sizeof(SK_MBUF), 0);
	if (pMsgBlock == NULL) {
		return (NULL);
	}
	pRlmtMbuf = (SK_MBUF*) pMsgBlock->data;
	skb_reserve(pMsgBlock, sizeof(SK_MBUF));
	pRlmtMbuf->pNext = NULL;
	pRlmtMbuf->pOs = pMsgBlock;
	pRlmtMbuf->pData = pMsgBlock->data;	/* Data buffer. */
	pRlmtMbuf->Size = BufferSize;		/* Data buffer size. */
	pRlmtMbuf->Length = 0;		/* Length of packet (<= Size). */
	return (pRlmtMbuf);

} /* SkDrvAllocRlmtMbuf */


/*****************************************************************************
 *
 *	SkDrvFreeRlmtMbuf - free an RLMT mbuf
 *
 * Description:
 *	This routine frees one or more RLMT mbuf(s).
 *
 * Context:
 *	runtime
 *
 * Returns:
 *	Nothing
 */
void  SkDrvFreeRlmtMbuf(
SK_AC		*pAC,		/* pointer to adapter context */
SK_IOC		IoC,		/* the IO-context */
SK_MBUF		*pMbuf)		/* size of the requested buffer */
{
SK_MBUF		*pFreeMbuf;
SK_MBUF		*pNextMbuf;

	pFreeMbuf = pMbuf;
	do {
		pNextMbuf = pFreeMbuf->pNext;
		DEV_KFREE_SKB_ANY(pFreeMbuf->pOs);
		pFreeMbuf = pNextMbuf;
	} while ( pFreeMbuf != NULL );
} /* SkDrvFreeRlmtMbuf */


/*****************************************************************************
 *
 *	SkOsGetTime - provide a time value
 *
 * Description:
 *	This routine provides a time value. The unit is 1/HZ (defined by Linux).
 *	It is not used for absolute time, but only for time differences.
 *
 *
 * Returns:
 *	Time value
 */
SK_U64 SkOsGetTime(SK_AC *pAC)
{
//	SK_U64	PrivateJiffies;

//	SkOsGetTimeCurrent(pAC, &PrivateJiffies);

//	return PrivateJiffies;
	return jiffies;
} /* SkOsGetTime */


/*****************************************************************************
 *
 *	SkPciReadCfgDWord - read a 32 bit value from pci config space
 *
 * Description:
 *	This routine reads a 32 bit value from the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciReadCfgDWord(
SK_AC *pAC,		/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U32 *pVal)		/* pointer to store the read value */
{
	pci_read_config_dword(pAC->PciDev, PciAddr, pVal);
	return(0);
} /* SkPciReadCfgDWord */


/*****************************************************************************
 *
 *	SkPciReadCfgWord - read a 16 bit value from pci config space
 *
 * Description:
 *	This routine reads a 16 bit value from the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciReadCfgWord(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U16 *pVal)		/* pointer to store the read value */
{
	pci_read_config_word(pAC->PciDev, PciAddr, pVal);
	return(0);
} /* SkPciReadCfgWord */


/*****************************************************************************
 *
 *	SkPciReadCfgByte - read a 8 bit value from pci config space
 *
 * Description:
 *	This routine reads a 8 bit value from the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciReadCfgByte(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U8 *pVal)		/* pointer to store the read value */
{
	pci_read_config_byte(pAC->PciDev, PciAddr, pVal);
	return(0);
} /* SkPciReadCfgByte */


/*****************************************************************************
 *
 *	SkPciWriteCfgDWord - write a 32 bit value to pci config space
 *
 * Description:
 *	This routine writes a 32 bit value to the pci configuration
 *	space.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciWriteCfgDWord(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U32 Val)		/* pointer to store the read value */
{
	pci_write_config_dword(pAC->PciDev, PciAddr, Val);
	return(0);
} /* SkPciWriteCfgDWord */


/*****************************************************************************
 *
 *	SkPciWriteCfgWord - write a 16 bit value to pci config space
 *
 * Description:
 *	This routine writes a 16 bit value to the pci configuration
 *	space. The flag PciConfigUp indicates whether the config space
 *	is accesible or must be set up first.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciWriteCfgWord(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U16 Val)		/* pointer to store the read value */
{
	pci_write_config_word(pAC->PciDev, PciAddr, Val);
	return(0);
} /* SkPciWriteCfgWord */


/*****************************************************************************
 *
 *	SkPciWriteCfgWord - write a 8 bit value to pci config space
 *
 * Description:
 *	This routine writes a 8 bit value to the pci configuration
 *	space. The flag PciConfigUp indicates whether the config space
 *	is accesible or must be set up first.
 *
 * Returns:
 *	0 - indicate everything worked ok.
 *	!= 0 - error indication
 */
int SkPciWriteCfgByte(
SK_AC *pAC,	/* Adapter Control structure pointer */
int PciAddr,		/* PCI register address */
SK_U8 Val)		/* pointer to store the read value */
{
	pci_write_config_byte(pAC->PciDev, PciAddr, Val);
	return(0);
} /* SkPciWriteCfgByte */


/*****************************************************************************
 *
 *	SkDrvEvent - handle driver events
 *
 * Description:
 *	This function handles events from all modules directed to the driver
 *
 * Context:
 *	Is called under protection of slow path lock.
 *
 * Returns:
 *	0 if everything ok
 *	< 0  on error
 *	
 */
int SkDrvEvent(
SK_AC     *pAC,    /* pointer to adapter context */
SK_IOC     IoC,    /* IO control context         */
SK_U32     Event,  /* event-id                   */
SK_EVPARA  Param)  /* event-parameter            */
{
	SK_MBUF         *pRlmtMbuf;   /* pointer to a rlmt-mbuf structure   */
	struct sk_buff  *pMsg;        /* pointer to a message block         */
	SK_BOOL          DualNet;
	SK_U32           Reason;
	KIRQL			 Irql;
	int              FromPort;    /* the port from which we switch away */
	int              ToPort;      /* the port we switch to              */
	int              Stat;
	DEV_NET 	*pNet = NULL;
#ifdef CONFIG_SK98LIN_NAPI
	int              WorkToDo = 1; /* min(*budget, dev->quota); */
	int              WorkDone = 0;
#endif

	switch (Event) {
	case SK_DRV_PORT_FAIL:
		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT FAIL EVENT, Port: %d\n", FromPort));
		if (FromPort == 0) {
			NbDebugPrint(0, ("%s: Port A failed.\n", pAC->dev[0]->name));
		} else {
			NbDebugPrint(0, ("%s: Port B failed.\n", pAC->dev[1]->name));
		}
		break;
	case SK_DRV_PORT_RESET:
		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT RESET EVENT, Port: %d ", FromPort));
		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					FromPort, SK_FALSE);
		KeAcquireSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			&Irql);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, IoC, FromPort, SK_STOP_ALL, SK_HARD_RST);
		} else {
			SkGeStopPort(pAC, IoC, FromPort, SK_STOP_ALL, SK_HARD_RST);
		}
		pAC->dev[Param.Para32[0]]->flags &= ~IFF_RUNNING;
		KeReleaseSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Irql);
		
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE);
#endif
			ClearTxRing(pAC, &pAC->TxPort[FromPort][TX_PRIO_LOW]);
		}
		KeAcquireSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			&Irql);

#ifdef USE_TIST_FOR_RESET
                if (pAC->GIni.GIYukon2) {
#ifdef Y2_RECOVERY
			/* for Yukon II we want to have tist enabled all the time */
			if (!SK_ADAPTER_WAITING_FOR_TIST(pAC)) {
				Y2_ENABLE_TIST(pAC->IoBase);
			}
#else
			/* make sure that we do not accept any status LEs from now on */
			if (SK_ADAPTER_WAITING_FOR_TIST(pAC)) {
#endif
				/* port already waiting for tist */
				SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DUMP,
					("Port %c is now waiting for specific Tist\n",
					'A' +  FromPort));
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_SPECIFIC_TIST,
					FromPort);
				/* get current timestamp */
				Y2_GET_TIST_LOW_VAL(pAC->IoBase, &pAC->MinTistLo);
				pAC->MinTistHi = pAC->GIni.GITimeStampCnt;
#ifndef Y2_RECOVERY
			} else {
				/* nobody is waiting yet */
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_ANY_TIST,
					FromPort);
				SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DUMP,
					("Port %c is now waiting for any Tist (0x%X)\n",
					'A' +  FromPort, pAC->AdapterResetState));
				/* start tist */
				Y2_ENABLE_TIST(pAC-IoBase);
			}
#endif
		}
#endif

#ifdef Y2_LE_CHECK
		/* mark entries invalid */
		pAC->LastPort = 3;
		pAC->LastOpc = 0xFF;
#endif
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStart(pAC, IoC, FromPort);
		} else {
			/* tschilling: Handling of return value inserted. */
			if (SkGeInitPort(pAC, IoC, FromPort)) {
				if (FromPort == 0) {
					NbDebugPrint(0, ("%s: SkGeInitPort A failed.\n", pAC->dev[0]->name));
				} else {
					NbDebugPrint(0, ("%s: SkGeInitPort B failed.\n", pAC->dev[1]->name));
				}
			}
			SkAddrMcUpdate(pAC,IoC, FromPort);
			PortReInitBmu(pAC, FromPort);
			SkGePollTxD(pAC, IoC, FromPort, SK_TRUE);
			CLEAR_AND_START_RX(FromPort);
			
			{
				UCHAR Val8;

				SK_IN8(pAC->TxPort[0][TX_PRIO_LOW].HwAddr, Q_CSR, &Val8);
				NbDebugPrint(3, ("pAC->TxPort[0][TX_PRIO_LOW]:QCSR = %08X\n", Val8));
				SK_IN8(pAC->IoBase, TxQueueAddr[0][TX_PRIO_LOW]+Q_CSR, &Val8);
				NbDebugPrint(3, ("TxQueueAddr[0][TX_PRIO_LOW]+Q_CSR = %08X\n", Val8));

				SK_IN8(pAC->RxPort[0].HwAddr, Q_CSR, &Val8);
				NbDebugPrint(3, ("pAC->RxPort[0]:QCSR = %08X\n", Val8));
				SK_IN8(pAC->IoBase, RxQueueAddr[0]+Q_CSR, &Val8);
				NbDebugPrint(3, ("RxQueueAddr[0]+Q_CSR = %08X\n", Val8));
			}
		}
		KeReleaseSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Irql);
		break;
	case SK_DRV_NET_UP:
		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("NET UP EVENT, Port: %d ", FromPort));
		SkAddrMcUpdate(pAC,IoC, FromPort); /* Mac update */
		if (DoPrintInterfaceChange) {
			NbDebugPrint(0, ("%s: network connection up using port %c\n",
				pAC->dev[FromPort]->name, 'A'+FromPort));

			/* tschilling: Values changed according to LinkSpeedUsed. */
			Stat = pAC->GIni.GP[FromPort].PLinkSpeedUsed;
			if (Stat == SK_LSPEED_STAT_10MBPS) {
				NbDebugPrint(0, ("    speed:           10\n"));
			} else if (Stat == SK_LSPEED_STAT_100MBPS) {
				NbDebugPrint(0, ("    speed:           100\n"));
			} else if (Stat == SK_LSPEED_STAT_1000MBPS) {
				NbDebugPrint(0, ("    speed:           1000\n"));
			} else {
				NbDebugPrint(0, ("    speed:           unknown\n"));
			}

			Stat = pAC->GIni.GP[FromPort].PLinkModeStatus;
			if ((Stat == SK_LMODE_STAT_AUTOHALF) ||
			    (Stat == SK_LMODE_STAT_AUTOFULL)) {
				NbDebugPrint(0, ("    autonegotiation: yes\n"));
			} else {
				NbDebugPrint(0, ("    autonegotiation: no\n"));
			}

			if ((Stat == SK_LMODE_STAT_AUTOHALF) ||
			    (Stat == SK_LMODE_STAT_HALF)) {
				NbDebugPrint(0, ("    duplex mode:     half\n"));
			} else {
				NbDebugPrint(0, ("    duplex mode:     full\n"));
			}

			Stat = pAC->GIni.GP[FromPort].PFlowCtrlStatus;
			if (Stat == SK_FLOW_STAT_REM_SEND ) {
				NbDebugPrint(0, ("    flowctrl:        remote send\n"));
			} else if (Stat == SK_FLOW_STAT_LOC_SEND ) {
				NbDebugPrint(0, ("    flowctrl:        local send\n"));
			} else if (Stat == SK_FLOW_STAT_SYMMETRIC ) {
				NbDebugPrint(0, ("    flowctrl:        symmetric\n"));
			} else {
				NbDebugPrint(0, ("    flowctrl:        none\n"));
			}
		
			/* tschilling: Check against CopperType now. */
			if ((pAC->GIni.GICopperType == SK_TRUE) &&
				(pAC->GIni.GP[FromPort].PLinkSpeedUsed ==
				SK_LSPEED_STAT_1000MBPS)) {
				Stat = pAC->GIni.GP[FromPort].PMSStatus;
				if (Stat == SK_MS_STAT_MASTER ) {
					NbDebugPrint(0, ("    role:            master\n"));
				} else if (Stat == SK_MS_STAT_SLAVE ) {
					NbDebugPrint(0, ("    role:            slave\n"));
				} else {
					NbDebugPrint(0, ("    role:            ???\n"));
				}
			}

			/* Display interrupt moderation informations */
			if (pAC->DynIrqModInfo.IntModTypeSelect == C_INT_MOD_STATIC) {
				NbDebugPrint(0, ("    irq moderation:  static (%d ints/sec)\n",
					pAC->DynIrqModInfo.MaxModIntsPerSec));
			} else if (pAC->DynIrqModInfo.IntModTypeSelect == C_INT_MOD_DYNAMIC) {
				NbDebugPrint(0, ("    irq moderation:  dynamic (%d ints/sec)\n",
					pAC->DynIrqModInfo.MaxModIntsPerSec));
			} else {
				NbDebugPrint(0, ("    irq moderation:  disabled\n"));
			}
	
#ifdef NETIF_F_TSO
			if (CHIP_ID_YUKON_2(pAC)) {
				if (pAC->dev[FromPort]->features & NETIF_F_TSO) {
					NbDebugPrint(0, ("    tcp offload:     enabled\n"));
				} else {
					NbDebugPrint(0, ("    tcp offload:     disabled\n"));
				}
			}
#endif

			if (pAC->dev[FromPort]->features & NETIF_F_SG) {
				NbDebugPrint(0, ("    scatter-gather:  enabled\n"));
			} else {
				NbDebugPrint(0, ("    scatter-gather:  disabled\n"));
			}

			if (pAC->dev[FromPort]->features & NETIF_F_IP_CSUM) {
				NbDebugPrint(0, ("    tx-checksum:     enabled\n"));
			} else {
				NbDebugPrint(0, ("    tx-checksum:     disabled\n"));
			}

			if (pAC->RxPort[FromPort].UseRxCsum) {
				NbDebugPrint(0, ("    rx-checksum:     enabled\n"));
			} else {
				NbDebugPrint(0, ("    rx-checksum:     disabled\n"));
			}
#ifdef CONFIG_SK98LIN_NAPI
			NbDebugPrint(0, ("    rx-polling:      enabled\n"));
#endif
			if (pAC->LowLatency) {
				NbDebugPrint(0, ("    low latency:     enabled\n"));
			}
		} else {
			DoPrintInterfaceChange = SK_TRUE;
		}
	
		if ((FromPort != pAC->ActivePort)&&(pAC->RlmtNets == 1)) {
			SkLocalEventQueue(pAC, SKGE_DRV, SK_DRV_SWITCH_INTERN,
						pAC->ActivePort, FromPort, SK_FALSE);
		}

		/* Inform the world that link protocol is up. */
		netif_wake_queue(pAC->dev[FromPort]);
		netif_carrier_on(pAC->dev[FromPort]);
		pAC->dev[FromPort]->flags |= IFF_RUNNING;
		break;
	case SK_DRV_NET_DOWN:	
		Reason   = Param.Para32[0];
		FromPort = Param.Para32[1];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("NET DOWN EVENT "));

		/* Stop queue and carrier */
		netif_stop_queue(pAC->dev[FromPort]);
		netif_carrier_off(pAC->dev[FromPort]);

		/* Print link change */
		if (DoPrintInterfaceChange) {
			if (pAC->dev[FromPort]->flags & IFF_RUNNING) {
				NbDebugPrint(0, ("%s: network connection down\n", 
					pAC->dev[FromPort]->name));
			}
		} else {
			DoPrintInterfaceChange = SK_TRUE;
		}
		pAC->dev[FromPort]->flags &= ~IFF_RUNNING;
		break;
	case SK_DRV_SWITCH_HARD:   /* FALL THRU */
	case SK_DRV_SWITCH_SOFT:   /* FALL THRU */
	case SK_DRV_SWITCH_INTERN: 
		FromPort = Param.Para32[0];
		ToPort   = Param.Para32[1];
		NbDebugPrint(0, ("%s: switching from port %c to port %c\n",
			pAC->dev[0]->name, 'A'+FromPort, 'A'+ToPort));
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT SWITCH EVENT, From: %d  To: %d (Pref %d) ",
			FromPort, ToPort, pAC->Rlmt.Net[0].PrefPort));
		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					FromPort, SK_FALSE);
		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					ToPort, SK_FALSE);
		KeAcquireSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			&Irql);
		KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		if (CHIP_ID_YUKON_2(pAC)) {
			SkY2PortStop(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
			SkY2PortStop(pAC, IoC, ToPort, SK_STOP_ALL, SK_SOFT_RST);
		}
		else {
			SkGeStopPort(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
			SkGeStopPort(pAC, IoC, ToPort, SK_STOP_ALL, SK_SOFT_RST);
		}
		KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		KeReleaseSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Irql);

		
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE, &WorkDone, WorkToDo);
			ReceiveIrq(pAC, &pAC->RxPort[ToPort], SK_FALSE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE); /* clears rx ring */
			ReceiveIrq(pAC, &pAC->RxPort[ToPort], SK_FALSE); /* clears rx ring */
#endif
			ClearTxRing(pAC, &pAC->TxPort[FromPort][TX_PRIO_LOW]);
			ClearTxRing(pAC, &pAC->TxPort[ToPort][TX_PRIO_LOW]);
		} 

		KeAcquireSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			&Irql);
		KeAcquireSpinLockAtDpcLevel(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		pAC->ActivePort = ToPort;

		/* tschilling: New common function with minimum size check. */
		DualNet = SK_FALSE;
		if (pAC->RlmtNets == 2) {
			DualNet = SK_TRUE;
		}
		
		if (SkGeInitAssignRamToQueues(
			pAC,
			pAC->ActivePort,
			DualNet)) {
			KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
			KeReleaseSpinLock(
				&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
				Irql);
			NbDebugPrint(0, ("SkGeInitAssignRamToQueues failed.\n"));
			break;
		}

		if (!CHIP_ID_YUKON_2(pAC)) {
			/* tschilling: Handling of return values inserted. */
			if (SkGeInitPort(pAC, IoC, FromPort) ||
				SkGeInitPort(pAC, IoC, ToPort)) {
				NbDebugPrint(0, ("%s: SkGeInitPort failed.\n", pAC->dev[0]->name));
			}
		}
		if (!CHIP_ID_YUKON_2(pAC)) {
			if (Event == SK_DRV_SWITCH_SOFT) {
				SkMacRxTxEnable(pAC, IoC, FromPort);
			}
			SkMacRxTxEnable(pAC, IoC, ToPort);
		}

		SkAddrSwap(pAC, IoC, FromPort, ToPort);
		SkAddrMcUpdate(pAC, IoC, FromPort);
		SkAddrMcUpdate(pAC, IoC, ToPort);

#ifdef USE_TIST_FOR_RESET
                if (pAC->GIni.GIYukon2) {
			/* make sure that we do not accept any status LEs from now on */
			SK_DBG_MSG(pAC, SK_DBGMOD_DRV, SK_DBGCAT_DUMP,
				("both Ports now waiting for specific Tist\n"));
			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_ANY_TIST,
				0);
			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_ANY_TIST,
				1);

			/* start tist */
			Y2_ENABLE_TIST(pAC->IoBase);
		}
#endif
		if (!CHIP_ID_YUKON_2(pAC)) {
			PortReInitBmu(pAC, FromPort);
			PortReInitBmu(pAC, ToPort);
			SkGePollTxD(pAC, IoC, FromPort, SK_TRUE);
			SkGePollTxD(pAC, IoC, ToPort, SK_TRUE);
			CLEAR_AND_START_RX(FromPort);
			CLEAR_AND_START_RX(ToPort);
		} else {
			SkY2PortStart(pAC, IoC, FromPort);
			SkY2PortStart(pAC, IoC, ToPort);
#ifdef SK_YUKON2
			/* in yukon-II always port 0 has to be started first */
			// SkY2PortStart(pAC, IoC, 0);
			// SkY2PortStart(pAC, IoC, 1);
#endif
		}
		KeReleaseSpinLockFromDpcLevel(&pAC->TxPort[ToPort][TX_PRIO_LOW].TxDesRingLock);
		KeReleaseSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Irql);
		break;
	case SK_DRV_RLMT_SEND:	 /* SK_MBUF *pMb */
		SK_DBG_MSG(NULL,SK_DBGMOD_DRV,SK_DBGCAT_DRV_EVENT,("RLS "));
		pRlmtMbuf = (SK_MBUF*) Param.pParaPtr;
		pMsg = (struct sk_buff*) pRlmtMbuf->pOs;
		skb_put(pMsg, pRlmtMbuf->Length);
		if (!CHIP_ID_YUKON_2(pAC)) {
			if (XmitFrame(pAC, &pAC->TxPort[pRlmtMbuf->PortIdx][TX_PRIO_LOW],
				pMsg) < 0) {				
				DEV_KFREE_SKB_ANY(pMsg);
			}
		} else {
			if (SkY2RlmtSend(pAC, pRlmtMbuf->PortIdx, pMsg) < 0) {
				DEV_KFREE_SKB_ANY(pMsg);
			}
		}
		break;
	case SK_DRV_TIMER:
		if (Param.Para32[0] == SK_DRV_MODERATION_TIMER) {
			/* check what IRQs are to be moderated */
			SkDimStartModerationTimer(pAC);
			SkDimModerate(pAC);
		} else {
			NbDebugPrint(0, ("Expiration of unknown timer\n"));
		}
		break;
	case SK_DRV_ADAP_FAIL:
#if (!defined (Y2_RECOVERY) && !defined (Y2_LE_CHECK))
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("ADAPTER FAIL EVENT\n"));
		NbDebugPrint(0, ("%s: Adapter failed.\n", pAC->dev[0]->name));
		SK_OUT32(pAC->IoBase, B0_IMSK, 0); /* disable interrupts */
		break;
#endif

#if (defined (Y2_RECOVERY) || defined (Y2_LE_CHECK))
	case SK_DRV_RECOVER:
		pNet = (DEV_NET *) pAC->dev[Param.Para32[0]]->priv;

		/* Recover already in progress */
		if (pNet->InRecover) {
			break;
		}

		netif_stop_queue(pAC->dev[0]); /* stop device if running */
		pNet->InRecover = SK_TRUE;

		FromPort = Param.Para32[0];
		SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
			("PORT RESET EVENT, Port: %d ", FromPort));

		/* Disable interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, 0);
		SK_OUT32(pAC->IoBase, B0_HWE_IMSK, 0);

		SkLocalEventQueue64(pAC, SKGE_PNMI, SK_PNMI_EVT_XMAC_RESET,
					FromPort, SK_FALSE);
		KeAcquireSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			&Irql);
		if (CHIP_ID_YUKON_2(pAC)) {
			if (pAC->GIni.GIMacsFound > 1) {
				SkY2PortStop(pAC, IoC, 0, SK_STOP_ALL, SK_SOFT_RST);
				SkY2PortStop(pAC, IoC, 1, SK_STOP_ALL, SK_SOFT_RST);
			} else {
				SkY2PortStop(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
			}
		} else {
			SkGeStopPort(pAC, IoC, FromPort, SK_STOP_ALL, SK_SOFT_RST);
		}
		pAC->dev[Param.Para32[0]]->flags &= ~IFF_RUNNING;
		KeReleaseSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Irql);
		
		if (!CHIP_ID_YUKON_2(pAC)) {
#ifdef CONFIG_SK98LIN_NAPI
			WorkToDo = 1;
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE, &WorkDone, WorkToDo);
#else
			ReceiveIrq(pAC, &pAC->RxPort[FromPort], SK_FALSE);
#endif
			ClearTxRing(pAC, &pAC->TxPort[FromPort][TX_PRIO_LOW]);
		}
		KeAcquireSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			&Irql);

#ifdef USE_TIST_FOR_RESET
		if (pAC->GIni.GIYukon2) {
#if 0
			/* make sure that we do not accept any status LEs from now on */
			Y2_ENABLE_TIST(pAC->IoBase);

			/* get current timestamp */
			Y2_GET_TIST_LOW_VAL(pAC->IoBase, &pAC->MinTistLo);
			pAC->MinTistHi = pAC->GIni.GITimeStampCnt;

			SK_SET_WAIT_BIT_FOR_PORT(
				pAC,
				SK_PSTATE_WAITING_FOR_SPECIFIC_TIST,
				FromPort);
#endif
			if (pAC->GIni.GIMacsFound > 1) {
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_ANY_TIST,
					0);
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_ANY_TIST,
					1);
			} else {
				SK_SET_WAIT_BIT_FOR_PORT(
					pAC,
					SK_PSTATE_WAITING_FOR_ANY_TIST,
					FromPort);
			}

			/* start tist */
                        Y2_ENABLE_TIST(pAC->IoBase);
		}
#endif

		/* Restart Receive BMU on Yukon-2 */
		if (HW_FEATURE(pAC, HWF_WA_DEV_4167)) {
			if (SkYuk2RestartRxBmu(pAC, IoC, FromPort)) {
				NbDebugPrint(0, ("%s: SkYuk2RestartRxBmu failed.\n", pAC->dev[FromPort]->name));
			}
		}

#ifdef Y2_LE_CHECK
		/* mark entries invalid */
		pAC->LastPort = 3;
		pAC->LastOpc = 0xFF;
#endif

#endif
		/* Restart ports but do not initialize PHY. */
		if (CHIP_ID_YUKON_2(pAC)) {
			if (pAC->GIni.GIMacsFound > 1) {
				SkY2PortStart(pAC, IoC, 0);
				SkY2PortStart(pAC, IoC, 1);
			} else {
				SkY2PortStart(pAC, IoC, FromPort);
			}
		} else {
			/* tschilling: Handling of return value inserted. */
			if (SkGeInitPort(pAC, IoC, FromPort)) {
				if (FromPort == 0) {
					NbDebugPrint(0, ("%s: SkGeInitPort A failed.\n", pAC->dev[0]->name));
				} else {
					NbDebugPrint(0, ("%s: SkGeInitPort B failed.\n", pAC->dev[1]->name));
				}
			}
			SkAddrMcUpdate(pAC,IoC, FromPort);
			PortReInitBmu(pAC, FromPort);
			SkGePollTxD(pAC, IoC, FromPort, SK_TRUE);
			CLEAR_AND_START_RX(FromPort);

			{
				UCHAR Val8;

				SK_IN8(pAC->TxPort[0][TX_PRIO_LOW].HwAddr, Q_CSR, &Val8);
				NbDebugPrint(3, ("pAC->TxPort[0][TX_PRIO_LOW]:QCSR = %08X\n", Val8));
				SK_IN8(pAC->IoBase, TxQueueAddr[0][TX_PRIO_LOW]+Q_CSR, &Val8);
				NbDebugPrint(3, ("TxQueueAddr[0][TX_PRIO_LOW]+Q_CSR = %08X\n", Val8));

				SK_IN8(pAC->RxPort[0].HwAddr, Q_CSR, &Val8);
				NbDebugPrint(3, ("pAC->RxPort[0]:QCSR = %08X\n", Val8));
				SK_IN8(pAC->IoBase, RxQueueAddr[0]+Q_CSR, &Val8);
				NbDebugPrint(3, ("RxQueueAddr[0]+Q_CSR = %08X\n", Val8));
			}
		}
		KeReleaseSpinLock(
			&pAC->TxPort[FromPort][TX_PRIO_LOW].TxDesRingLock,
			Irql);

		/* Map any waiting RX buffers to HW */
		FillReceiveTableYukon2(pAC, pAC->IoBase, FromPort);

		pNet->InRecover = SK_FALSE;
		/* enable Interrupts */
		SK_OUT32(pAC->IoBase, B0_IMSK, pAC->GIni.GIValIrqMask);
		SK_OUT32(pAC->IoBase, B0_HWE_IMSK, IRQ_HWE_MASK);
		netif_wake_queue(pAC->dev[FromPort]);
		break;
	default:
		break;
	}
	SK_DBG_MSG(NULL, SK_DBGMOD_DRV, SK_DBGCAT_DRV_EVENT,
		("END EVENT\n "));

	return (0);
} /* SkDrvEvent */


/******************************************************************************
 *
 *	SkLocalEventQueue()	-	add event to queue
 *
 * Description:
 *	This function adds an event to the event queue and run the
 *	SkEventDispatcher. At least Init Level 1 is required to queue events,
 *	but will be scheduled add Init Level 2.
 *
 * returns:
 *	nothing
 */
void SkLocalEventQueue(
SK_AC *pAC,		/* Adapters context */
SK_U32 Class,		/* Event Class */
SK_U32 Event,		/* Event to be queued */
SK_U32 Param1,		/* Event parameter 1 */
SK_U32 Param2,		/* Event parameter 2 */
SK_BOOL Dispatcher)	/* Dispatcher flag:
			 *	TRUE == Call SkEventDispatcher
			 *	FALSE == Don't execute SkEventDispatcher
			 */
{
	SK_EVPARA 	EvPara;
	EvPara.Para32[0] = Param1;
	EvPara.Para32[1] = Param2;
	

	NbDebugPrint(3, ("SkLocalEventQueue: class = %d, event = %d\n", Class, Event));
	if (Class == SKGE_PNMI) {
		SkPnmiEvent(	pAC,
				pAC->IoBase,
				Event,
				EvPara);
	} else {
		SkEventQueue(	pAC,
				Class,
				Event,
				EvPara);
	}

	/* Run the dispatcher */
	if (Dispatcher) {
		SkEventDispatcher(pAC, pAC->IoBase);
	}

}

/******************************************************************************
 *
 *	SkLocalEventQueue64()	-	add event to queue (64bit version)
 *
 * Description:
 *	This function adds an event to the event queue and run the
 *	SkEventDispatcher. At least Init Level 1 is required to queue events,
 *	but will be scheduled add Init Level 2.
 *
 * returns:
 *	nothing
 */
void SkLocalEventQueue64(
SK_AC *pAC,		/* Adapters context */
SK_U32 Class,		/* Event Class */
SK_U32 Event,		/* Event to be queued */
SK_U64 Param,		/* Event parameter */
SK_BOOL Dispatcher)	/* Dispatcher flag:
			 *	TRUE == Call SkEventDispatcher
			 *	FALSE == Don't execute SkEventDispatcher
			 */
{
	SK_EVPARA 	EvPara;
	EvPara.Para64 = Param;


	NbDebugPrint(3, ("SkLocalEventQueue64: class = %d, event = %d\n", Class, Event));
	if (Class == SKGE_PNMI) {
		SkPnmiEvent(	pAC,
				pAC->IoBase,
				Event,
				EvPara);
	} else {
		SkEventQueue(	pAC,
				Class,
				Event,
				EvPara);
	}

	/* Run the dispatcher */
	if (Dispatcher) {
		SkEventDispatcher(pAC, pAC->IoBase);
	}

}


/*****************************************************************************
 *
 *	SkErrorLog - log errors
 *
 * Description:
 *	This function logs errors to the system buffer and to the console
 *
 * Returns:
 *	0 if everything ok
 *	< 0  on error
 *	
 */
void SkErrorLog(
SK_AC	*pAC,
int	ErrClass,
int	ErrNum,
char	*pErrorMsg)
{
char	ClassStr[80];

	switch (ErrClass) {
	case SK_ERRCL_OTHER:
		strcpy(ClassStr, "Other error");
		break;
	case SK_ERRCL_CONFIG:
		strcpy(ClassStr, "Configuration error");
		break;
	case SK_ERRCL_INIT:
		strcpy(ClassStr, "Initialization error");
		break;
	case SK_ERRCL_NORES:
		strcpy(ClassStr, "Out of resources error");
		break;
	case SK_ERRCL_SW:
		strcpy(ClassStr, "internal Software error");
		break;
	case SK_ERRCL_HW:
		strcpy(ClassStr, "Hardware failure");
		break;
	case SK_ERRCL_COMM:
		strcpy(ClassStr, "Communication error");
		break;
	}
	NbDebugPrint(0, ("%s: -- ERROR --\n        Class:  %s\n"
		"        Nr:  0x%x\n        Msg:  %s\n", pAC->dev[0]->name,
		ClassStr, ErrNum, pErrorMsg));

} /* SkErrorLog */

/*****************************************************************************
 *
 *	SkDrvEnterDiagMode - handles DIAG attach request
 *
 * Description:
 *	Notify the kernel to NOT access the card any longer due to DIAG
 *	Deinitialize the Card
 *
 * Returns:
 *	int
 */
int SkDrvEnterDiagMode(
SK_AC   *pAc)   /* pointer to adapter context */
{
	SK_AC   *pAC  = NULL;
	DEV_NET *pNet = NULL;

	pNet = (DEV_NET *) pAc->dev[0]->priv;
	pAC = pNet->pAC;

	SK_MEMCPY(&(pAc->PnmiBackup), &(pAc->PnmiStruct), 
			sizeof(SK_PNMI_STRUCT_DATA));

	pAC->DiagModeActive = DIAG_ACTIVE;
	if (pAC->BoardLevel > SK_INIT_DATA) {
		if (netif_running(pAC->dev[0])) {
			pAC->WasIfUp[0] = SK_TRUE;
			pAC->DiagFlowCtrl = SK_TRUE; /* for SkGeClose      */
			DoPrintInterfaceChange = SK_FALSE;
			SkDrvDeInitAdapter(pAC, 0);  /* performs SkGeClose */
		} else {
			pAC->WasIfUp[0] = SK_FALSE;
		}

		if (pNet != (DEV_NET *) pAc->dev[1]->priv) {
			pNet = (DEV_NET *) pAc->dev[1]->priv;
			if (netif_running(pAC->dev[1])) {
				pAC->WasIfUp[1] = SK_TRUE;
				pAC->DiagFlowCtrl = SK_TRUE; /* for SkGeClose */
				DoPrintInterfaceChange = SK_FALSE;
				SkDrvDeInitAdapter(pAC, 1);  /* do SkGeClose  */
			} else {
				pAC->WasIfUp[1] = SK_FALSE;
			}
		}
		pAC->BoardLevel = SK_INIT_DATA;
	}
	return(0);
}

/*****************************************************************************
 *
 *	SkDrvLeaveDiagMode - handles DIAG detach request
 *
 * Description:
 *	Notify the kernel to may access the card again after use by DIAG
 *	Initialize the Card
 *
 * Returns:
 * 	int
 */
int SkDrvLeaveDiagMode(
SK_AC   *pAc)   /* pointer to adapter control context */
{ 
	SK_MEMCPY(&(pAc->PnmiStruct), &(pAc->PnmiBackup), 
			sizeof(SK_PNMI_STRUCT_DATA));
	pAc->DiagModeActive    = DIAG_NOTACTIVE;
	pAc->Pnmi.DiagAttached = SK_DIAG_IDLE;
	if (pAc->WasIfUp[0] == SK_TRUE) {
		pAc->DiagFlowCtrl = SK_TRUE; /* for SkGeClose */
		DoPrintInterfaceChange = SK_FALSE;
		SkDrvInitAdapter(pAc, 0);    /* first device  */
	}
	if (pAc->WasIfUp[1] == SK_TRUE) {
		pAc->DiagFlowCtrl = SK_TRUE; /* for SkGeClose */
		DoPrintInterfaceChange = SK_FALSE;
		SkDrvInitAdapter(pAc, 1);    /* second device */
	}
	return(0);
}

/*****************************************************************************
 *
 *	ParseDeviceNbrFromSlotName - Evaluate PCI device number
 *
 * Description:
 * 	This function parses the PCI slot name information string and will
 *	retrieve the devcie number out of it. The slot_name maintianed by
 *	linux is in the form of '02:0a.0', whereas the first two characters 
 *	represent the bus number in hex (in the sample above this is 
 *	pci bus 0x02) and the next two characters the device number (0x0a).
 *
 * Returns:
 *	SK_U32: The device number from the PCI slot name
 */ 

static SK_U32 ParseDeviceNbrFromSlotName(
const char *SlotName)   /* pointer to pci slot name eg. '02:0a.0' */
{
	char	*CurrCharPos	= (char *) SlotName;
	int	FirstNibble	= -1;
	int	SecondNibble	= -1;
	SK_U32	Result		=  0;

	while (*CurrCharPos != '\0') {
		if (*CurrCharPos == ':') { 
			while (*CurrCharPos != '.') {
				CurrCharPos++;  
				if (	(*CurrCharPos >= '0') && 
					(*CurrCharPos <= '9')) {
					if (FirstNibble == -1) {
						/* dec. value for '0' */
						FirstNibble = *CurrCharPos - 48;
					} else {
						SecondNibble = *CurrCharPos - 48;
					}  
				} else if (	(*CurrCharPos >= 'a') && 
						(*CurrCharPos <= 'f')  ) {
					if (FirstNibble == -1) {
						FirstNibble = *CurrCharPos - 87; 
					} else {
						SecondNibble = *CurrCharPos - 87; 
					}
				} else {
					Result = 0;
				}
			}

			Result = FirstNibble;
			Result = Result << 4; /* first nibble is higher one */
			Result = Result | SecondNibble;
		}
		CurrCharPos++;   /* next character */
	}
	return (Result);
}

/****************************************************************************
 *
 *	SkDrvDeInitAdapter - deinitialize adapter (this function is only 
 *				called if Diag attaches to that card)
 *
 * Description:
 *	Close initialized adapter.
 *
 * Returns:
 *	0 - on success
 *	error code - on error
 */
static int SkDrvDeInitAdapter(
SK_AC   *pAC,		/* pointer to adapter context   */
int      devNbr)	/* what device is to be handled */
{
	struct SK_NET_DEVICE *dev;

	dev = pAC->dev[devNbr];

	/*
	** Function SkGeClose() uses MOD_DEC_USE_COUNT (2.2/2.4)
	** or module_put() (2.6) to decrease the number of users for
	** a device, but if a device is to be put under control of 
	** the DIAG, that count is OK already and does not need to 
	** be adapted! Hence the opposite MOD_INC_USE_COUNT or 
	** try_module_get() needs to be used again to correct that.
	*/
	MOD_INC_USE_COUNT;

	if (SkGeClose(dev) != 0) {
		MOD_DEC_USE_COUNT;
		return (-1);
	}
	return (0);

} /* SkDrvDeInitAdapter() */

/****************************************************************************
 *
 *	SkDrvInitAdapter - Initialize adapter (this function is only 
 *				called if Diag deattaches from that card)
 *
 * Description:
 *	Close initialized adapter.
 *
 * Returns:
 *	0 - on success
 *	error code - on error
 */
static int SkDrvInitAdapter(
SK_AC   *pAC,		/* pointer to adapter context   */
int      devNbr)	/* what device is to be handled */
{
	struct SK_NET_DEVICE *dev;

	dev = pAC->dev[devNbr];

	if (SkGeOpen(dev) != 0) {
		return (-1);
	} else {
		/*
		** Function SkGeOpen() uses MOD_INC_USE_COUNT (2.2/2.4) 
		** or try_module_get() (2.6) to increase the number of 
		** users for a device, but if a device was just under 
		** control of the DIAG, that count is OK already and 
		** does not need to be adapted! Hence the opposite 
		** MOD_DEC_USE_COUNT or module_put() needs to be used 
		** again to correct that.
		*/
		MOD_DEC_USE_COUNT;
	}

	/*
	** Use correct MTU size and indicate to kernel TX queue can be started
	*/ 
	if (SkGeChangeMtu(dev, dev->mtu) != 0) {
		return (-1);
	} 
	return (0);

} /* SkDrvInitAdapter */

#ifdef DEBUG
/****************************************************************************/
/* "debug only" section *****************************************************/
/****************************************************************************/

/*****************************************************************************
 *
 *	DumpMsg - print a frame
 *
 * Description:
 *	This function prints frames to the system logfile/to the console.
 *
 * Returns: N/A
 *	
 */
static void DumpMsg(
struct sk_buff *skb,  /* linux' socket buffer  */
char           *str)  /* additional msg string */
{
//	int msglen = (skb->len > 64) ? 64 : skb->len;
	int msglen = 64;

	if (skb == NULL) {
		NbDebugPrint(0, ("DumpMsg(): NULL-Message\n"));
		return;
	}

	if (skb->data == NULL) {
		NbDebugPrint(0, ("DumpMsg(): Message empty\n"));
		return;
	}

	NbDebugPrint(0, ("DumpMsg: data: %p\n", skb->data));
	NbDebugPrint(0, ("--- Begin of message from %s , len %d (from %d) ----\n", 
		str, msglen, skb->len));
	DumpData((char *)skb->data, msglen);
	NbDebugPrint(0, ("------- End of message ---------\n"));
} /* DumpMsg */

/*****************************************************************************
 *
 *	DumpData - print a data area
 *
 * Description:
 *	This function prints a area of data to the system logfile/to the
 *	console.
 *
 * Returns: N/A
 *	
 */
static void DumpData(
char  *p,     /* pointer to area containing the data */
int    size)  /* the size of that data area in bytes */
{
	register int  i;
	int           haddr = 0, addr = 0;
	char          hex_buffer[180] = { '\0' };
	char          asc_buffer[180] = { '\0' };
	char          HEXCHAR[] = "0123456789ABCDEF";

	for (i=0; i < size; ) {
		if (*p >= '0' && *p <='z') {
			asc_buffer[addr] = *p;
		} else {
			asc_buffer[addr] = '.';
		}
		addr++;
		asc_buffer[addr] = 0;
		hex_buffer[haddr] = HEXCHAR[(*p & 0xf0) >> 4];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[*p & 0x0f];
		haddr++;
		hex_buffer[haddr] = ' ';
		haddr++;
		hex_buffer[haddr] = 0;
		p++;
		i++;
		if (i%16 == 0) {
			NbDebugPrint(0, ("%s  %s\n", hex_buffer, asc_buffer));
			addr = 0;
			haddr = 0;
		}
	}
} /* DumpData */


/*****************************************************************************
 *
 *	DumpLong - print a data area as long values
 *
 * Description:
 *	This function prints a long variable to the system logfile/to the
 *	console.
 *
 * Returns: N/A
 *	
 */
static void DumpLong(
char  *pc,    /* location of the variable to print */
int    size)  /* how large is the variable?        */
{
	register int   i;
	int            haddr = 0;
	char           hex_buffer[180] = { '\0' };
	char           HEXCHAR[] = "0123456789ABCDEF";
	long          *p = (long*) pc;
	int            l;

	for (i=0; i < size; ) {
		l = (long) *p;
		hex_buffer[haddr] = HEXCHAR[(l >> 28) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 24) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 20) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 16) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 12) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 8) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[(l >> 4) & 0xf];
		haddr++;
		hex_buffer[haddr] = HEXCHAR[l & 0x0f];
		haddr++;
		hex_buffer[haddr] = ' ';
		haddr++;
		hex_buffer[haddr] = 0;
		p++;
		i++;
		if (i%8 == 0) {
			NbDebugPrint(0, ("%4x %s\n", (i-8)*4, hex_buffer));
			haddr = 0;
		}
	}
	NbDebugPrint(0, ("------------------------\n"));
} /* DumpLong */

static void skge_dump_state(SK_AC *pAC)
{
	SK_U8	Byte, Addr[SK_MAC_ADDR_LEN];
	SK_U16	Word;
	SK_U32	DWord;	
	SK_IOC	IoC = pAC->IoBase;
	int i;
	int Port = 0;
	SK_GEPORT	*pPrt;

	pPrt = &pAC->GIni.GP[Port];

	/*
	* Configuration Space header
	* Since this module is used for different OS', those may be
	* duplicate on some of them (e.g. Linux). But to keep the
	* common source, we have to live with this...
	*/

	SK_IN16(IoC, PCI_C(pAC, PCI_VENDOR_ID), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_VENDOR_ID) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_DEVICE_ID), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_DEVICE_ID) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_COMMAND), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_COMMAND) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_STATUS), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_STATUS) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PCI_REV_ID), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_REV_ID:PCI_CLASS_CODE) [%08x]\n", DWord));

	SK_IN8(IoC, PCI_C(pAC, PCI_CACHE_LSZ), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_CACHE_LSZ) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_LAT_TIM), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_LAT_TIM) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_HEADER_T), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_HEADER_T) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_BIST), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_BIST) [%02x]\n", Byte));

	SK_IN32(IoC, PCI_C(pAC, PCI_BASE_1ST), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_BASE_1ST) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PCI_BASE_2ND), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_BASE_2ND) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PCI_SUB_VID), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_SUB_VID) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_SUB_ID), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_SUB_ID) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PCI_BASE_ROM), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_BASE_ROM) [%08x]\n", DWord));

	SK_IN8(IoC, PCI_C(pAC, PCI_CAP_PTR), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_CAP_PTR) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_IRQ_LINE), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_IRQ_LINE) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_IRQ_PIN), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_IRQ_PIN) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_MIN_GNT), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MIN_GNT) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_MAX_LAT), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MAX_LAT) [%02x]\n", Byte));

	SK_IN32(IoC, PCI_C(pAC, PCI_OUR_REG_1), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_OUR_REG_1) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PCI_OUR_REG_2), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_OUR_REG_2) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PCI_PM_CAP_ID), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_PM_CAP_ID) [%02x]\n", Word));

	SK_IN8(IoC, PCI_C(pAC, PCI_PM_NITEM), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_PM_NITEM) [%02x]\n", Byte));

	SK_IN16(IoC, PCI_C(pAC, PCI_PM_CAP_REG), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_PM_CAP_REG) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_PM_CTL_STS), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_PM_CTL_STS) [%04x]\n", Word));

	SK_IN8(IoC, PCI_C(pAC, PCI_PM_DAT_REG), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_PM_DAT_REG) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_VPD_CAP_ID), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_VPD_CAP_ID) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_VPD_NITEM), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_VPD_CAP_ID) [%02x]\n", Byte));
	
	SK_IN16(IoC, PCI_C(pAC, PCI_VPD_ADR_REG), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_VPD_ADR_REG) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PCI_VPD_DAT_REG), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_VPD_DAT_REG) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PCI_SER_LD_CTRL), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_SER_LD_CTRL) [%04x]\n", Word));

	SK_IN8(IoC, PCI_C(pAC, PCI_MSI_CAP_ID), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MSI_CAP_ID) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_MSI_NITEM), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MSI_NITEM) [%02x]\n", Byte));

	SK_IN16(IoC, PCI_C(pAC, PCI_MSI_CTRL), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MSI_CTRL) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PCI_MSI_ADR_LO), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MSI_ADR_LO) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PCI_MSI_ADR_HI), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MSI_ADR_HI) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PCI_MSI_DATA), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_MSI_DATA) [%04x]\n", Word));

	SK_IN8(IoC, PCI_C(pAC, PCI_X_CAP_ID), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_X_CAP_ID) [%02x]\n", Byte));

	SK_IN8(IoC, PCI_C(pAC, PCI_X_NITEM), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_X_NITEM) [%02x]\n", Byte));

	SK_IN16(IoC, PCI_C(pAC, PCI_X_COMMAND), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_X_COMMAND) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PCI_X_PE_STAT), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_X_PE_STAT) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PCI_CAL_CTRL), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_CAL_CTRL) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_CAL_STAT), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_CAL_STAT) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PCI_DISC_CNT), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_DISC_CNT) [%04x]\n", Word));

	SK_IN8(IoC, PCI_C(pAC, PCI_RETRY_CNT), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_RETRY_CNT) [%02x]\n", Byte));

	SK_IN32(IoC, PCI_C(pAC, PCI_OUR_STATUS), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PCI_OUR_STATUS) [%08x]\n", DWord));

	SK_IN8(IoC, PCI_C(pAC, PEX_CAP_ID), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_CAP_ID) [%02x]\n", Byte));
	
	SK_IN8(IoC, PCI_C(pAC, PEX_NITEM), &Byte);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_NITEM) [%02x]\n", Byte));

	SK_IN16(IoC, PCI_C(pAC, PEX_CAP_REG), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_CAP_REG) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PEX_DEV_CAP), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_DEV_CAP) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PEX_DEV_CTRL), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_DEV_CTRL) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PEX_DEV_STAT), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_DEV_STAT) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PEX_LNK_CAP), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_LNK_CAP) [%08x]\n", DWord));

	SK_IN16(IoC, PCI_C(pAC, PEX_LNK_CTRL), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_LNK_CTRL) [%04x]\n", Word));

	SK_IN16(IoC, PCI_C(pAC, PEX_LNK_STAT), &Word);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_LNK_STAT) [%04x]\n", Word));

	SK_IN32(IoC, PCI_C(pAC, PEX_ADV_ERR_REP), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_ADV_ERR_REP) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_UNC_ERR_STAT), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_UNC_ERR_STAT) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_UNC_ERR_MASK), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_UNC_ERR_MASK) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_UNC_ERR_SEV), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_UNC_ERR_SEV) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_COR_ERR_STAT), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_COR_ERR_STAT) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_COR_ERR_MASK), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_COR_ERR_MASK) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_ADV_ERR_CAP_C), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_ADV_ERR_CAP_C) [%08x]\n", DWord));

	SK_IN32(IoC, PCI_C(pAC, PEX_HEADER_LOG), &DWord);
	NbDebugPrint(0, ("DEBUG: PCI_C(PEX_HEADER_LOG) [%08x]\n", DWord));

	/*
	*	Bank 0
	*/

	SK_IN8(IoC, B0_RAP, &Byte);
	NbDebugPrint(0, ("DEBUG: B0_RAP [%02x]\n", Byte));

	SK_IN16(IoC, B0_CTST, &Word);
	NbDebugPrint(0, ("DEBUG: B0_CTST [%04x]\n", Word));

	SK_IN8(IoC, B0_LED, &Byte);
	NbDebugPrint(0, ("DEBUG: B0_LED [%02x]\n", Byte));

	SK_IN8(IoC, B0_POWER_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: B0_POWER_CTRL [%02x]\n", Byte));

	SK_IN32(IoC, B0_ISRC, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_ISRC [%08x]\n", DWord));

	SK_IN32(IoC, B0_IMSK, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_IMSK [%08x]\n", DWord));

	SK_IN32(IoC, B0_HWE_ISRC, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_HWE_ISRC [%08x]\n", DWord));

	SK_IN32(IoC, B0_HWE_IMSK, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_HWE_IMSK [%08x]\n", DWord));

	SK_IN32(IoC, B0_SP_ISRC, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_SP_ISRC [%08x]\n", DWord));

	SK_IN32(IoC, B0_Y2_SP_ISRC2, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_Y2_SP_ISRC2 [%08x]\n", DWord));

	SK_IN32(IoC, B0_Y2_SP_ISRC3, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_Y2_SP_ISRC3 [%08x]\n", DWord));

	SK_IN32(IoC, B0_Y2_SP_EISR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_Y2_SP_EISR [%08x]\n", DWord));

	SK_IN32(IoC, B0_Y2_SP_LISR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_Y2_SP_LISR [%08x]\n", DWord));

	SK_IN32(IoC, B0_Y2_SP_ICR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_Y2_SP_ICR [%08x]\n", DWord));

	SK_IN16(IoC, B0_XM1_IMSK, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM1_IMSK [%04x]\n", Word));

	SK_IN16(IoC, B0_XM1_ISRC, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM1_ISRC [%04x]\n", Word));

	SK_IN16(IoC, B0_XM1_PHY_ADDR, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM1_PHY_ADDR [%04x]\n", Word));

	SK_IN16(IoC, B0_XM1_PHY_DATA, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM1_PHY_DATA [%04x]\n", Word));

	SK_IN16(IoC, B0_XM2_IMSK, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM2_IMSK [%04x]\n", Word));

	SK_IN16(IoC, B0_XM2_ISRC, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM2_ISRC [%04x]\n", Word));

	SK_IN16(IoC, B0_XM2_PHY_DATA, &Word);
	NbDebugPrint(0, ("DEBUG: B0_XM2_PHY_DATA [%04x]\n", Word));

	SK_IN32(IoC, B0_R1_CSR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_R1_CSR [%08x]\n", DWord));

	SK_IN32(IoC, B0_XS1_CSR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_R2_CSR [%08x]\n", DWord));

	SK_IN32(IoC, B0_XS1_CSR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_R2_CSR [%08x]\n", DWord));

	SK_IN32(IoC, B0_XA1_CSR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_XA1_CSR [%08x]\n", DWord));

	SK_IN32(IoC, B0_XA2_CSR, &DWord);
	NbDebugPrint(0, ("DEBUG: B0_XA2_CSR [%08x]\n", DWord));

	/*
	*	Bank 2
	*/

	for(i=0;i<SK_MAC_ADDR_LEN;i++) {
		SK_IN8(IoC, B2_MAC_1 + i, &Addr[i]);
	}
	NbDebugPrint(0, ("DEBUG: B2_MAC_1: [%02X:%02X:%02X:%02X:%02X:%02X]\n", Addr[0], Addr[1], Addr[2], Addr[3], Addr[4], Addr[5]));

	for(i=0;i<SK_MAC_ADDR_LEN;i++) {
		SK_IN8(IoC, B2_MAC_2 + i, &Addr[i]);
	}
	NbDebugPrint(0, ("DEBUG: B2_MAC_2: [%02X:%02X:%02X:%02X:%02X:%02X]\n", Addr[0], Addr[1], Addr[2], Addr[3], Addr[4], Addr[5]));

	for(i=0;i<SK_MAC_ADDR_LEN;i++) {
		SK_IN8(IoC, B2_MAC_3 + i, &Addr[i]);
	}
	NbDebugPrint(0, ("DEBUG: B2_MAC_3: [%02X:%02X:%02X:%02X:%02X:%02X]\n", Addr[0], Addr[1], Addr[2], Addr[3], Addr[4], Addr[5]));

	SK_IN8(IoC, B2_CONN_TYP, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_CONN_TYP [%02x]\n", Byte));

	SK_IN8(IoC, B2_PMD_TYP, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_PMD_TYP [%02x]\n", Byte));

	SK_IN8(IoC, B2_MAC_CFG, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_MAC_CFG [%02x]\n", Byte));

	SK_IN8(IoC, B2_CHIP_ID, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_CHIP_ID [%02x]\n", Byte));

	SK_IN8(IoC, B2_E_0, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_E_0 [%02x]\n", Byte));

	SK_IN8(IoC, B2_E_1, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_E_1 [%02x]\n", Byte));

	SK_IN8(IoC, B2_E_2, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_E_2 [%02x]\n", Byte));

	SK_IN8(IoC, B2_Y2_CLK_GATE, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_Y2_CLK_GATE [%02x]\n", Byte));

	SK_IN8(IoC, B2_Y2_HW_RES, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_Y2_HW_RES [%02x]\n", Byte));

	SK_IN8(IoC, B2_E_3, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_E_3 [%02x]\n", Byte));

	SK_IN32(IoC, B2_FAR, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_FAR [%08x]\n", DWord));

	SK_IN8(IoC, B2_FDP, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_FDP [%02x]\n", Byte));

	SK_IN32(IoC, B2_Y2_CLK_CTRL, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_Y2_CLK_CTRL [%08x]\n", DWord));

	SK_IN8(IoC, B2_LD_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_LD_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, B2_LD_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_LD_TEST [%02x]\n", Byte));

	SK_IN32(IoC, B2_TI_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_TI_INI [%08x]\n", DWord));

	SK_IN32(IoC, B2_TI_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_TI_VAL [%08x]\n", DWord));

	SK_IN8(IoC, B2_TI_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_TI_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, B2_TI_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_TI_TEST [%02x]\n", Byte));

	SK_IN32(IoC, B2_IRQM_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_IRQM_INI [%08x]\n", DWord));

	SK_IN32(IoC, B2_IRQM_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_IRQM_VAL [%08x]\n", DWord));

	SK_IN8(IoC, B2_IRQM_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_IRQM_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, B2_IRQM_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_IRQM_TEST [%02x]\n", Byte));

	SK_IN32(IoC, B2_IRQM_MSK, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_IRQM_MSK [%08x]\n", DWord));

	SK_IN32(IoC, B2_IRQM_HWE_MSK, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_IRQM_HWE_MSK [%08x]\n", DWord));

	SK_IN8(IoC, B2_TST_CTRL1, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_TST_CTRL1 [%02x]\n", Byte));

	SK_IN8(IoC, B2_TST_CTRL2, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_TST_CTRL2 [%02x]\n", Byte));

	SK_IN32(IoC, B2_GP_IO, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_GP_IO [%08x]\n", DWord));

	SK_IN32(IoC, B2_I2C_CTRL, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_I2C_CTRL [%08x]\n", DWord));

	SK_IN32(IoC, B2_I2C_DATA, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_I2C_DATA [%08x]\n", DWord));

	SK_IN32(IoC, B2_I2C_IRQ, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_I2C_IRQ [%08x]\n", DWord));

	SK_IN32(IoC, B2_I2C_SW, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_I2C_SW [%08x]\n", DWord));

	SK_IN32(IoC, B2_BSC_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_BSC_INI [%08x]\n", DWord));

	SK_IN32(IoC, B2_BSC_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: B2_BSC_VAL [%08x]\n", DWord));

	SK_IN8(IoC, B2_BSC_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_BSC_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, B2_BSC_STAT, &Byte);
	NbDebugPrint(0, ("DEBUG: B2_BSC_STAT [%02x]\n", Byte));

	SK_IN16(IoC, B2_BSC_TST, &Word);
	NbDebugPrint(0, ("DEBUG: B2_BSC_TST [%04x]\n", Word));

	SK_IN16(IoC, Y2_PEX_PHY_DATA, &Word);
	NbDebugPrint(0, ("DEBUG: Y2_PEX_PHY_DATA [%04x]\n", Word));

	SK_IN16(IoC, Y2_PEX_PHY_ADDR, &Word);
	NbDebugPrint(0, ("DEBUG: Y2_PEX_PHY_ADDR [%04x]\n", Word));


#if 0
	/*
	*	Bank 3
	*/

	SK_IN32(IoC, SELECT_RAM_BUFFER(Port, B3_RAM_ADDR), &DWord);
	NbDebugPrint(0, ("DEBUG: B3_RAM_ADDR [%08x]\n", DWord));

	SK_IN32(IoC, SELECT_RAM_BUFFER(Port, B3_RAM_DATA_LO), &DWord);
	NbDebugPrint(0, ("DEBUG: B3_RAM_DATA_LO [%08x]\n", DWord));

	SK_IN32(IoC, SELECT_RAM_BUFFER(Port, B3_RAM_DATA_HI), &DWord);
	NbDebugPrint(0, ("DEBUG: B3_RAM_DATA_HI [%08x]\n", DWord));


	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_WTO_R1), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_WTO_R1 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_WTO_XA1), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_WTO_XA1 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_WTO_XS1), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_WTO_XS1 [%02x]\n", Byte));


	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_RTO_R1), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_RTO_R1 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_RTO_XA1), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_RTO_XA1 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_RTO_XS1), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_RTO_XS1 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_WTO_R2), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_WTO_R2 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_WTO_XA2), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_WTO_XA2 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_WTO_XS2), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_WTO_XS2 [%02x]\n", Byte));

		SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_RTO_R2), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_RTO_R2 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_RTO_XA2), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_RTO_XA2 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_RTO_XS2), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_RTO_XS2 [%02x]\n", Byte));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_TO_VAL), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_TO_VAL [%02x]\n", Byte));

	SK_IN16(IoC, SELECT_RAM_BUFFER(Port, B3_RI_CTRL), &Word);
	NbDebugPrint(0, ("DEBUG: B3_RI_CTRL [%04x]\n", Word));

	SK_IN8(IoC, SELECT_RAM_BUFFER(Port, B3_RI_TEST), &Byte);
	NbDebugPrint(0, ("DEBUG: B3_RI_TEST [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOINI_RX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOINI_RX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOINI_RX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOINI_RX2 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOINI_TX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOINI_TX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOINI_TX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOINI_TX2 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOVAL_RX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOVAL_RX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOVAL_RX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOVAL_RX2 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOVAL_TX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOVAL_TX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_TOVAL_TX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOVAL_TX2 [%02x]\n", Byte));

	SK_IN16(IoC, B3_MA_TO_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: B3_MA_TO_CTRL [%04x]\n", Word));

	SK_IN16(IoC, B3_MA_TO_TEST, &Word);
	NbDebugPrint(0, ("DEBUG: B3_MA_TO_TEST [%04x]\n", Word));


	SK_IN8(IoC, B3_MA_RCINI_RX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCINI_RX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCINI_RX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_TOINI_RX2 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCINI_TX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCINI_TX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCINI_TX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCINI_TX2 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCVAL_RX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCVAL_RX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCVAL_RX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCVAL_RX2 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCVAL_TX1, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCVAL_TX1 [%02x]\n", Byte));

	SK_IN8(IoC, B3_MA_RCVAL_TX2, &Byte);
	NbDebugPrint(0, ("DEBUG: B3_MA_RCVAL_TX2 [%02x]\n", Byte));

	SK_IN16(IoC, B3_MA_RC_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: B3_MA_RC_CTRL [%04x]\n", Word));

	SK_IN16(IoC, B3_MA_RC_TEST, &Word);
	NbDebugPrint(0, ("DEBUG: B3_MA_RC_TEST [%04x]\n", Word));


	SK_IN16(IoC, B3_PA_TOINI_RX1, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOINI_RX1 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOINI_RX2, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOINI_RX2 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOINI_TX1, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOINI_TX1 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOINI_TX2, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOINI_TX2 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOVAL_RX1, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOVAL_RX1 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOVAL_RX2, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOVAL_RX2 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOVAL_TX1, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOVAL_TX1 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TOVAL_TX2, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TOVAL_TX2 [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_CTRL [%04x]\n", Word));

	SK_IN16(IoC, B3_PA_TEST, &Word);
	NbDebugPrint(0, ("DEBUG: B3_PA_TEST [%04x]\n", Word));

	/*
	*	Bank 4 - 5
	*/

	SK_IN32(IoC, TXA_ITI_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: TXA_ITI_INI [%08x]\n", DWord));

	SK_IN32(IoC, TXA_ITI_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: TXA_ITI_VAL [%08x]\n", DWord));

	SK_IN32(IoC, TXA_LIM_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: TXA_LIM_INI [%08x]\n", DWord));

	SK_IN32(IoC, TXA_LIM_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: TXA_LIM_VAL [%08x]\n", DWord));

	SK_IN8(IoC, TXA_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: TXA_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, TXA_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: TXA_TEST [%02x]\n", Byte));

	SK_IN8(IoC, TXA_STAT, &Byte);
	NbDebugPrint(0, ("DEBUG: TXA_STAT [%02x]\n", Byte));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_D), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_D) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_D+4), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_D+4) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_D+8), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_D+8) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_D+12), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_D+12) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_DA_L), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_DA_L) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_DA_H), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_DA_H) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_AC_L), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_AC_L) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_AC_H), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_AC_H) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_BC), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_BC) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_CSR), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_CSR) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_F), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_F) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_T1), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T1) [%08x]\n", DWord));

	SK_IN8(IoC, Q_ADDR(pPrt->PRxQOff, Q_T1_TR), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T1_TR) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PRxQOff, Q_T1_WR), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T1_WR) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PRxQOff, Q_T1_RD), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T1_RD) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PRxQOff, Q_T1_SV), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T1_SV) [%02x]\n", Byte));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_T2), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T2) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PRxQOff, Q_T3), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PRxQOff, Q_T3) [%08x]\n", DWord));


	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_START), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_START) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_END), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_END) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_WP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_RP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_RX_UTPP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_RX_UTPP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_RX_LTPP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_RX_LTPP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_RX_UTHP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_RX_UTHP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_RX_LTHP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_RX_LTHP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_PC), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_PC) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_LEV), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_LEV) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PRxQOff, RB_CTRL), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_CTRL) [%08x]\n", DWord));

	SK_IN8(IoC, RB_ADDR(pPrt->PRxQOff, RB_TST1), &Byte);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_TST1) [%02x]\n", Byte));

	SK_IN8(IoC, RB_ADDR(pPrt->PRxQOff, RB_TST2), &Byte);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PRxQOff, RB_TST2) [%02x]\n", Byte));



	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_D), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_D) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_D+4), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_D+4) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_D+8), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_D+8) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_D+12), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_D+12) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_DA_L), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_DA_L) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_DA_H), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_DA_H) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_AC_L), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_AC_L) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_AC_H), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_AC_H) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_BC), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_BC) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_CSR), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_CSR) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_F), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_F) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_T1), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T1) [%08x]\n", DWord));

	SK_IN8(IoC, Q_ADDR(pPrt->PXsQOff, Q_T1_TR), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T1_TR) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PXsQOff, Q_T1_WR), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T1_WR) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PXsQOff, Q_T1_RD), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T1_RD) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PXsQOff, Q_T1_SV), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T1_SV) [%02x]\n", Byte));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_T2), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T2) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXsQOff, Q_T3), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXsQOff, Q_T3) [%08x]\n", DWord));

	
	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_START), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_START) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_END), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_END) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_WP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_RP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_RX_UTPP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_RX_UTPP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_RX_LTPP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_RX_LTPP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_RX_UTHP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_RX_UTHP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_RX_LTHP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_RX_LTHP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_PC), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_PC) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_LEV), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_LEV) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXsQOff, RB_CTRL), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_CTRL) [%08x]\n", DWord));

	SK_IN8(IoC, RB_ADDR(pPrt->PXsQOff, RB_TST1), &Byte);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_TST1) [%02x]\n", Byte));

	SK_IN8(IoC, RB_ADDR(pPrt->PXsQOff, RB_TST2), &Byte);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXsQOff, RB_TST2) [%02x]\n", Byte));
	
	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_D), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_D) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_D+4), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_D+4) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_D+8), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_D+8) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_D+12), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_D+12) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_DA_L), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_DA_L) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_DA_H), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_DA_H) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_AC_L), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_AC_L) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_AC_H), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_AC_H) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_BC), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_BC) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_CSR), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_CSR) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_F), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_F) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_T1), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T1) [%08x]\n", DWord));

	SK_IN8(IoC, Q_ADDR(pPrt->PXaQOff, Q_T1_TR), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T1_TR) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PXaQOff, Q_T1_WR), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T1_WR) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PXaQOff, Q_T1_RD), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T1_RD) [%02x]\n", Byte));

	SK_IN8(IoC, Q_ADDR(pPrt->PXaQOff, Q_T1_SV), &Byte);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T1_SV) [%02x]\n", Byte));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_T2), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T2) [%08x]\n", DWord));

	SK_IN32(IoC, Q_ADDR(pPrt->PXaQOff, Q_T3), &DWord);
	NbDebugPrint(0, ("DEBUG: Q_ADDR(pPrt->PXaQOff, Q_T3) [%08x]\n", DWord));


	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_START), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_START) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_END), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_END) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_WP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_RP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_RX_UTPP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_RX_UTPP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_RX_LTPP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_RX_LTPP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_RX_UTHP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_RX_UTHP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_RX_LTHP), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_RX_LTHP) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_PC), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_PC) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_LEV), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_LEV) [%08x]\n", DWord));

	SK_IN32(IoC, RB_ADDR(pPrt->PXaQOff, RB_CTRL), &DWord);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_CTRL) [%08x]\n", DWord));

	SK_IN8(IoC, RB_ADDR(pPrt->PXaQOff, RB_TST1), &Byte);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_TST1) [%02x]\n", Byte));

	SK_IN8(IoC, RB_ADDR(pPrt->PXaQOff, RB_TST2), &Byte);
	NbDebugPrint(0, ("DEBUG: RB_ADDR(pPrt->PXaQOff, RB_TST2) [%02x]\n", Byte));

	/*
	*	Bank 24
	*/

	SK_IN32(IoC, MR_ADDR(Port, RX_MFF_EA), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_EA) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_MFF_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_WP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_MFF_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_RP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_MFF_PC), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_PC) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_MFF_LEV), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_LEV) [%08x]\n", DWord));

	SK_IN16(IoC, MR_ADDR(Port, RX_MFF_CTRL1), &Word);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_CTRL1) [%04x]\n", Word));

	SK_IN8(IoC, MR_ADDR(Port, RX_MFF_STAT_TO), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_STAT_TO) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, RX_MFF_TIST_TO), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_TIST_TO) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, RX_MFF_CTRL2), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_CTRL2) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, RX_MFF_TST1), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_TST1) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, RX_MFF_TST2), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_MFF_TST2) [%02x]\n", Byte));

	SK_IN32(IoC, MR_ADDR(Port, RX_LED_INI), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_LED_INI) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_LED_VAL), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_LED_VAL) [%08x]\n", DWord));

	SK_IN8(IoC, MR_ADDR(Port, RX_LED_CTRL), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_LED_CTRL) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, RX_LED_TST), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_LED_TST) [%02x]\n", Byte));

	SK_IN32(IoC, MR_ADDR(Port, LNK_SYNC_INI), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, LNK_SYNC_INI) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, LNK_SYNC_VAL), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, LNK_SYNC_VAL) [%08x]\n", DWord));

	SK_IN8(IoC, MR_ADDR(Port, LNK_SYNC_CTRL), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, LNK_SYNC_CTRL) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, LNK_SYNC_TST), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, LNK_SYNC_TST) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, LNK_LED_REG), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, LNK_LED_REG) [%02x]\n", Byte));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_EA), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_EA) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_AF_THR), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_AF_THR) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_CTRL_T), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_CTRL_T) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_FL_MSK), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_FL_MSK) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_FL_THR), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_FL_THR) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_TR_THR), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_TR_THR) [%08x]\n", DWord));

	SK_IN8(IoC, MR_ADDR(Port, RX_GMF_UP_THR), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_UP_THR) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, RX_GMF_LP_THR), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_LP_THR) [%02x]\n", Byte));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_VLAN), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_VLAN) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_WP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_WLEV), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_WLEV) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_RP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, RX_GMF_RLEV), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, RX_GMF_RLEV) [%08x]\n", DWord));


	/*
	*	Bank 26
	*/

	SK_IN32(IoC, MR_ADDR(Port, TX_MFF_EA), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_EA) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_MFF_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_WP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_MFF_WSP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_WSP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_MFF_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_RP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_MFF_PC), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_PC) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_MFF_LEV), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_LEV) [%08x]\n", DWord));

	SK_IN16(IoC, MR_ADDR(Port, TX_MFF_CTRL1), &Word);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_CTRL1) [%04x]\n", Word));

	SK_IN8(IoC, MR_ADDR(Port, TX_MFF_WAF), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_WAF) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, TX_MFF_CTRL2), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_CTRL2) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, TX_MFF_TST1), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_TST1) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, TX_MFF_TST2), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_MFF_TST2) [%02x]\n", Byte));

	SK_IN32(IoC, MR_ADDR(Port, TX_LED_INI), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_LED_INI) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_LED_VAL), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_LED_VAL) [%08x]\n", DWord));

	SK_IN8(IoC, MR_ADDR(Port, TX_LED_CTRL), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_LED_CTRL) [%02x]\n", Byte));

	SK_IN8(IoC, MR_ADDR(Port, TX_LED_TST), &Byte);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_LED_TST) [%02x]\n", Byte));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_EA), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_EA) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_AE_THR), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_AE_THR) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_CTRL_T), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_CTRL_T) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_VLAN), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_VLAN) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_WP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_WP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_WSP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_WSP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_WLEV), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_WLEV) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_RP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_RP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_RSTP), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_RSTP) [%08x]\n", DWord));

	SK_IN32(IoC, MR_ADDR(Port, TX_GMF_RLEV), &DWord);
	NbDebugPrint(0, ("DEBUG: MR_ADDR(Port, TX_GMF_RLEV) [%08x]\n", DWord));

#endif
	/*
	*	Bank 28
	*/

	SK_IN32(IoC, B28_DPT_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_DPT_INI [%08x]\n", DWord));

	SK_IN32(IoC, B28_DPT_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_DPT_VAL [%08x]\n", DWord));

	SK_IN8(IoC, B28_DPT_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: B28_DPT_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, B28_DPT_TST, &Byte);
	NbDebugPrint(0, ("DEBUG: B28_DPT_TST [%02x]\n", Byte));

	SK_IN32(IoC, GMAC_TI_ST_VAL, &DWord);
	NbDebugPrint(0, ("DEBUG: GMAC_TI_ST_VAL [%08x]\n", DWord));

	SK_IN8(IoC, GMAC_TI_ST_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: GMAC_TI_ST_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, GMAC_TI_ST_TST, &Byte);
	NbDebugPrint(0, ("DEBUG: GMAC_TI_ST_TST [%02x]\n", Byte));

	SK_IN32(IoC, POLL_CTRL, &DWord);
	NbDebugPrint(0, ("DEBUG: POLL_CTRL [%08x]\n", DWord));

	SK_IN16(IoC, POLL_LAST_IDX, &Word);
	NbDebugPrint(0, ("DEBUG: POLL_LAST_IDX [%04x]\n", Word));

	SK_IN32(IoC, POLL_LIST_ADDR_LO, &DWord);
	NbDebugPrint(0, ("DEBUG: POLL_LIST_ADDR_LO [%08x]\n", DWord));

	SK_IN32(IoC, POLL_LIST_ADDR_HI, &DWord);
	NbDebugPrint(0, ("DEBUG: POLL_LIST_ADDR_HI [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_SMB_CONFIG, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_SMB_CONFIG [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_SMB_CSD_REG, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_SMB_CSD_REG [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_ASF_IRQ_V_BASE, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_ASF_IRQ_V_BASE [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_ASF_STAT_CMD, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_ASF_STAT_CMD [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_ASF_HOST_COM, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_ASF_HOST_COM [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_DATA_REG_1, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_DATA_REG_1 [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_DATA_REG_2, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_DATA_REG_2 [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_DATA_REG_3, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_DATA_REG_3 [%08x]\n", DWord));

	SK_IN32(IoC, B28_Y2_DATA_REG_4, &DWord);
	NbDebugPrint(0, ("DEBUG: B28_Y2_DATA_REG_4 [%08x]\n", DWord));

	/*
	*	Bank 29
	*/
	
	SK_IN32(IoC, STAT_CTRL, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_CTRL [%08x]\n", DWord));

	SK_IN16(IoC, STAT_LAST_IDX, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_LAST_IDX [%04x]\n", Word));

	SK_IN32(IoC, STAT_LIST_ADDR_LO, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_LIST_ADDR_LO [%08x]\n", DWord));

	SK_IN32(IoC, STAT_LIST_ADDR_HI, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_LIST_ADDR_HI [%08x]\n", DWord));

	SK_IN16(IoC, STAT_TXA1_RIDX, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_TXA1_RIDX [%04x]\n", Word));

	SK_IN16(IoC, STAT_TXS1_RIDX, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_TXS1_RIDX [%04x]\n", Word));

	SK_IN16(IoC, STAT_TXA2_RIDX, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_TXA2_RIDX [%04x]\n", Word));

	SK_IN16(IoC, STAT_TXS2_RIDX, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_TXS2_RIDX [%04x]\n", Word));

	SK_IN16(IoC, STAT_TX_IDX_TH, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_TX_IDX_TH [%04x]\n", Word));

	SK_IN16(IoC, STAT_PUT_IDX, &Word);
	NbDebugPrint(0, ("DEBUG: STAT_PUT_IDX [%04x]\n", Word));

	SK_IN8(IoC, STAT_FIFO_WP, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_WP [%02x]\n", Byte));

	SK_IN8(IoC, STAT_FIFO_RP, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_RP [%02x]\n", Byte));

	SK_IN8(IoC, STAT_FIFO_RSP, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_RSP [%02x]\n", Byte));

	SK_IN8(IoC, STAT_FIFO_LEVEL, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_LEVEL [%02x]\n", Byte));

	SK_IN8(IoC, STAT_FIFO_SHLVL, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_SHLVL [%02x]\n", Byte));

	SK_IN8(IoC, STAT_FIFO_WM, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_WM [%02x]\n", Byte));

	SK_IN8(IoC, STAT_FIFO_ISR_WM, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_FIFO_ISR_WM [%02x]\n", Byte));

	SK_IN32(IoC, STAT_LEV_TIMER_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_LEV_TIMER_INI [%08x]\n", DWord));

	SK_IN32(IoC, STAT_LEV_TIMER_CNT, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_LEV_TIMER_CNT [%08x]\n", DWord));

	SK_IN8(IoC, STAT_LEV_TIMER_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_LEV_TIMER_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, STAT_LEV_TIMER_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_LEV_TIMER_TEST [%02x]\n", Byte));

	SK_IN32(IoC, STAT_TX_TIMER_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_TX_TIMER_INI [%08x]\n", DWord));

	SK_IN32(IoC, STAT_TX_TIMER_CNT, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_TX_TIMER_CNT [%08x]\n", DWord));

	SK_IN8(IoC, STAT_TX_TIMER_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_TX_TIMER_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, STAT_TX_TIMER_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_TX_TIMER_TEST [%02x]\n", Byte));

	SK_IN32(IoC, STAT_ISR_TIMER_INI, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_ISR_TIMER_INI [%08x]\n", DWord));

	SK_IN32(IoC, STAT_ISR_TIMER_CNT, &DWord);
	NbDebugPrint(0, ("DEBUG: STAT_ISR_TIMER_CNT [%08x]\n", DWord));

	SK_IN8(IoC, STAT_ISR_TIMER_CTRL, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_ISR_TIMER_CTRL [%02x]\n", Byte));

	SK_IN8(IoC, STAT_ISR_TIMER_TEST, &Byte);
	NbDebugPrint(0, ("DEBUG: STAT_ISR_TIMER_TEST [%02x]\n", Byte));

	/*
	*	Bank 30
	*/

	SK_IN32(IoC, GMAC_CTRL, &DWord);
	NbDebugPrint(0, ("DEBUG: GMAC_CTRL [%08x]\n", DWord));

	SK_IN32(IoC, GPHY_CTRL, &DWord);
	NbDebugPrint(0, ("DEBUG: GPHY_CTRL [%08x]\n", DWord));

	SK_IN8(IoC, GMAC_IRQ_SRC, &Byte);
	NbDebugPrint(0, ("DEBUG: GMAC_IRQ_SRC [%02x]\n", Byte));

	SK_IN8(IoC, GMAC_IRQ_MSK, &Byte);
	NbDebugPrint(0, ("DEBUG: GMAC_IRQ_MSK [%02x]\n", Byte));

	SK_IN16(IoC, GMAC_LINK_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: GMAC_LINK_CTRL [%04x]\n", Word));

	SK_IN16(IoC, WOL_CTRL_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: WOL_CTRL_STAT [%04x]\n", Word));

	SK_IN8(IoC, WOL_MATCH_CTL, &Byte);
	NbDebugPrint(0, ("DEBUG: WOL_MATCH_CTL [%02x]\n", Byte));

	SK_IN8(IoC, WOL_MATCH_RES, &Byte);
	NbDebugPrint(0, ("DEBUG: WOL_MATCH_RES [%02x]\n", Byte));

	SK_IN32(IoC, WOL_MAC_ADDR_LO, &DWord);
	NbDebugPrint(0, ("DEBUG: WOL_MAC_ADDR_LO [%08x]\n", DWord));

	SK_IN16(IoC, WOL_MAC_ADDR_HI, &Word);
	NbDebugPrint(0, ("DEBUG: WOL_MAC_ADDR_HI [%04x]\n", Word));

	SK_IN8(IoC, WOL_PATT_PME, &Byte);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_PME [%02x]\n", Byte));

	SK_IN8(IoC, WOL_PATT_ASFM, &Byte);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_ASFM [%02x]\n", Byte));

	SK_IN8(IoC, WOL_PATT_RPTR, &Byte);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_RPTR [%02x]\n", Byte));

	SK_IN32(IoC, WOL_PATT_LEN_LO, &DWord);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_LEN_LO [%08x]\n", DWord));

	SK_IN32(IoC, WOL_PATT_LEN_HI, &DWord);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_LEN_HI [%08x]\n", DWord));

	SK_IN32(IoC, WOL_PATT_CNT_0, &DWord);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_CNT_0 [%08x]\n", DWord));

	SK_IN32(IoC, WOL_PATT_CNT_4, &DWord);
	NbDebugPrint(0, ("DEBUG: WOL_PATT_CNT_4 [%08x]\n", DWord));




	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_CTRL [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_STAT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_ID0, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_ID0 [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_ID1, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_ID1 [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_AUNE_ADV, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_AUNE_ADV [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_AUNE_LP, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_AUNE_LP [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_AUNE_EXP, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_AUNE_EXP [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_NEPG, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_NEPG [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_NEPG_LP, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_NEPG_LP [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_1000T_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_1000T_CTRL [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_1000T_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_1000T_STAT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_EXT_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_EXT_STAT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_PHY_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_PHY_CTRL [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_PHY_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_PHY_STAT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_INT_MASK, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_INT_MASK [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_INT_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_INT_STAT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_EXT_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_EXT_CTRL [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_RXE_CNT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_RXE_CNT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_PORT_IRQ, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_PORT_IRQ [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_LED_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_LED_CTRL [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_LED_OVER, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_LED_OVER [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_EXT_CTRL_2, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_EXT_CTRL_2 [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_EXT_P_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_EXT_P_STAT [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_CABLE_DIAG, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_CABLE_DIAG [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_PAGE_ADDR, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_PAGE_ADDR [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_PAGE_DATA, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_PAGE_DATA [%04x]\n", Word));

	SkGmPhyRead(pAC, IoC, Port, PHY_MARV_PAGE_DATA, &Word);
	NbDebugPrint(0, ("DEBUG: PHY_MARV_PAGE_DATA [%04x]\n", Word));


	GM_IN16(IoC, Port, GM_GP_STAT, &Word);
	NbDebugPrint(0, ("DEBUG: GM_GP_STAT [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_GP_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: GM_GP_CTRL [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TX_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TX_CTRL [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_RX_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: GM_RX_CTRL [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TX_FLOW_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TX_FLOW_CTRL [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TX_PARAM, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TX_PARAM [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SERIAL_MODE, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SERIAL_MODE [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SRC_ADDR_1L, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SRC_ADDR_1L [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SRC_ADDR_1M, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SRC_ADDR_1M [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SRC_ADDR_1H, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SRC_ADDR_1H [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SRC_ADDR_2L, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SRC_ADDR_2L [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SRC_ADDR_2M, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SRC_ADDR_2M [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SRC_ADDR_2H, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SRC_ADDR_2H [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_MC_ADDR_H1, &Word);
	NbDebugPrint(0, ("DEBUG: GM_MC_ADDR_H1 [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_MC_ADDR_H2, &Word);
	NbDebugPrint(0, ("DEBUG: GM_MC_ADDR_H2 [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_MC_ADDR_H3, &Word);
	NbDebugPrint(0, ("DEBUG: GM_MC_ADDR_H3 [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_MC_ADDR_H4, &Word);
	NbDebugPrint(0, ("DEBUG: GM_MC_ADDR_H4 [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TX_IRQ_SRC, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TX_IRQ_SRC [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_RX_IRQ_SRC, &Word);
	NbDebugPrint(0, ("DEBUG: GM_RX_IRQ_SRC [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TR_IRQ_SRC, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TR_IRQ_SRC [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TX_IRQ_MSK, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TX_IRQ_MSK [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_RX_IRQ_MSK, &Word);
	NbDebugPrint(0, ("DEBUG: GM_RX_IRQ_MSK [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_TR_IRQ_MSK, &Word);
	NbDebugPrint(0, ("DEBUG: GM_TR_IRQ_MSK [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SMI_CTRL, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SMI_CTRL [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_SMI_DATA, &Word);
	NbDebugPrint(0, ("DEBUG: GM_SMI_DATA [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_PHY_ADDR, &Word);
	NbDebugPrint(0, ("DEBUG: GM_PHY_ADDR [%04x]\n", Word));

	GM_IN16(IoC, Port, GM_MIB_CNT_BASE, &Word);
	NbDebugPrint(0, ("DEBUG: GM_MIB_CNT_BASE [%04x]\n", Word));

	read_config_header(pAC->PciDev);
	display_config_header(pAC->PciDev);
}

#endif

/*******************************************************************************
 *
 * End of file
 *
 ******************************************************************************/

