#ifndef __NIC_H
#define __NIC_H

#include "pci.h"

#define INTERRUPT_TIME	1	//10 * 1000 // * 100 ns 
#define TRANSMIT_TIME	1

#define PRIMARY_VECTOR_BASE	0x30

#define inb(a) 			(UCHAR) ScsiPortReadPortUchar((PUCHAR)(a))
#define outb(a,b) 		ScsiPortWritePortUchar((PUCHAR)(b), (UCHAR)(a))
#define inw(a) 			(USHORT) ScsiPortReadPortUshort((PUSHORT)(a))
#define outw(a,b) 		ScsiPortWritePortUshort((PUSHORT)(b), (USHORT)(a))
#define inl(a) 			(ULONG) ScsiPortReadPortUlong((PULONG)(a))
#define outl(a,b) 		ScsiPortWritePortUlong((PULONG)(b), (ULONG)(a))
#define insw(a,b,c) 	ScsiPortReadPortBufferUshort((PUSHORT)(a), (PUSHORT)(b), (ULONG)c)
#define outsw(a,b,c) 	ScsiPortWritePortBufferUshort((PUSHORT)(a), (PUSHORT)(b), (ULONG)c) 

#define insl(a,b,c) \
{	\
	unsigned int i ;	\
	unsigned int *p ; \
	p = (b) ;	\
	for( i = 0 ; i < (c) ; i ++ ) { \
		*(p) = inl((a)) ;	\
		(p)++;	\
	} \
} \

#define outsl(a,b,c) \
{	\
	unsigned int i ;	\
	unsigned int *p ; \
	p = (b) ;	\
	for( i = 0 ; i < (c) ; i ++ ) { \
		outl(*(p),(a)) ;	\
		(p)++;	\
	} \
} \

#define le32_to_cpu(val) (val)
#define cpu_to_le32(val) (val)

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

#define	jiffies	(CurrentTime().QuadPart)

#ifndef __ENABLE_LOADER__
BOOLEAN
HalEnableSystemInterrupt(
    IN ULONG Vector,
    IN KIRQL Irql,
    IN KINTERRUPT_MODE InterruptMode
    );

VOID
HalDisableSystemInterrupt (
    IN ULONG Vector,
    IN KIRQL Irql
    );
#endif

/* Standard interface flags (netdevice->flags). */
#define IFF_UP          0x1             /* interface is up              */
#define IFF_BROADCAST   0x2             /* broadcast address valid      */
#define IFF_DEBUG       0x4             /* turn on debugging            */
#define IFF_LOOPBACK    0x8             /* is a loopback net            */
#define IFF_POINTOPOINT 0x10            /* interface is has p-p link    */
#define IFF_NOTRAILERS  0x20            /* avoid use of trailers        */
#define IFF_RUNNING     0x40            /* resources allocated          */
#define IFF_NOARP       0x80            /* no ARP protocol              */
#define IFF_PROMISC     0x100           /* receive all packets          */
#define IFF_ALLMULTI    0x200           /* receive all multicast packets*/

#define IFF_MASTER      0x400           /* master of a load balancer    */
#define IFF_SLAVE       0x800           /* slave of a load balancer     */

#define IFF_MULTICAST   0x1000          /* Supports multicast           */

#define IFF_VOLATILE    (IFF_LOOPBACK|IFF_POINTOPOINT|IFF_BROADCAST|IFF_MASTER|IFF_SLAVE|IFF_RUNNING)

#define IFF_PORTSEL     0x2000          /* can set media type           */
#define IFF_AUTOMEDIA   0x4000          /* auto media select active     */
#define IFF_DYNAMIC     0x8000          /* dialup device with changing addresses*/

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif