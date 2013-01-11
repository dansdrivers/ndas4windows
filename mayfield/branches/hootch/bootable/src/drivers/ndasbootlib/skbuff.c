/*
 *	Routines having to do with the 'struct sk_buff' memory handlers.
 *
 *	Authors:	Alan Cox <iiitac@pyr.swan.ac.uk>
 *			Florian La Roche <rzsfl@rz.uni-sb.de>
 *
 *	Version:	$Id: skbuff.c,v 1.55 1999/02/23 08:12:27 davem Exp $
 *
 *	Fixes:	
 *		Alan Cox	:	Fixed the worst of the load balancer bugs.
 *		Dave Platt	:	Interrupt stacking fix.
 *	Richard Kooijman	:	Timestamp fixes.
 *		Alan Cox	:	Changed buffer format.
 *		Alan Cox	:	destructor hook for AF_UNIX etc.
 *		Linus Torvalds	:	Better skb_clone.
 *		Alan Cox	:	Added skb_copy.
 *		Alan Cox	:	Added all the changed routines Linus
 *					only put in the headers
 *		Ray VanTassle	:	Fixed --skb->lock in free
 *		Alan Cox	:	skb_copy copy arp field
 *		Andi Kleen	:	slabified it.
 *
 *	NOTE:
 *		The __skb_ routines should be called with interrupts 
 *	disabled, or you better be *real* sure that the operation is atomic 
 *	with respect to whatever list is being frobbed (e.g. via lock_sock()
 *	or via disabling bottom half handlers, etc).
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

/*
 *	The functions in this file will not compile correctly with gcc 2.4.x
 */
#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include "ndasboot.h"
#include "linux2win.h"
#include "debug.h"
#include "skbuff.h"
#include "netdevice.h"
#include "errno.h"

KSPIN_LOCK skb_queue_lock; 

/*
 *	Resource tracking variables
 */
static long net_skbcount = 0;
static long net_allocs = 0;
static long net_fails  = 0;

long alloc_skbs = 0;
long freed_skbs = 0;
long cloned_skbs = 0;
long clone_freed_skbs = 0;
long skbs_copy = 0;


void skbuff_init(void)
{
    KeInitializeSpinLock(&skb_queue_lock);
}


/*
 *	Keep out-of-line to prevent kernel bloat.
 *	__builtin_return_address is not used because it is not always
 *	reliable. 
 */

void skb_over_panic(struct sk_buff *skb, int sz, void *here)
{
	NbDebugPrint(0, ("skput:over: %p:%d put:%d dev:%s", 
		here, skb->len, sz, skb->dev ? skb->dev->name : "<NULL>"));
}

void skb_under_panic(struct sk_buff *skb, int sz, void *here)
{
	NbDebugPrint(0, ("skput:under: %p:%d put:%d dev:%s",
		here, skb->len, sz, skb->dev ? skb->dev->name : "<NULL>"));
}

void show_net_buffers(void)
{
	NbDebugPrint(1, ("Networking buffers in use          : %u\n",net_skbcount));
	NbDebugPrint(1, ("Total network buffer allocations   : %u\n",net_allocs));
	NbDebugPrint(1, ("Total failed network buffer allocs : %u\n",net_fails));
}

/* 	Allocate a new skbuff. We do this ourselves so we can fill in a few
 *	'private' fields and also do memory statistics to find all the
 *	[BEEP] leaks.
 * 
 */

struct sk_buff *alloc_skb(unsigned int size,int gfp_mask)
{
	struct sk_buff *skb;
	UCHAR *data;	

	UNREFERENCED_PARAMETER(gfp_mask);

	/* Get the HEAD */
	NbDebugPrint(3,("H"));
//	skb = kmalloc(sizeof(struct sk_buff), gfp_mask);
	skb = (struct sk_buff *)ExAllocatePoolWithTag(NonPagedPool, sizeof(struct sk_buff), LSMP_PTAG_SKBUFF_HEADER);
	if (skb == NULL) {
		NbDebugPrint(0, ("No sk buffer head !!\n"));
		goto nohead;
	}

	memset(skb, 0, sizeof(struct sk_buff));

	/* Get the DATA. Size must match skb_add_mtu(). */
	size = ((size + 15) & ~15); 

	NbDebugPrint(3,("D"));
//	data = kmalloc(size + sizeof(struct skb_shared_info), gfp_mask);
	data = (UCHAR *)ExAllocatePoolWithTag(NonPagedPool, size + sizeof(struct skb_shared_info), LSMP_PTAG_SKBUFF_DATA);

	if (data == NULL) {
		NbDebugPrint(0, ("No data !!\n"));
		goto nodata;
	}



	/* Note that this counter is useless now - you can just look in the
	 * skbuff_head entry in /proc/slabinfo. We keep it only for emergency
	 * cases.
	 */
	InterlockedIncrement(&net_allocs);

//	NbDebugPrint(0, ("skb = %08X, data = %08X\n", skb, data));

	skb->truesize = size;

	InterlockedIncrement(&net_skbcount);

	/* Load the data pointers. */
	skb->head = data;
	skb->data = data;
	skb->tail = data;
	skb->end = data + size;

	/* Set up other state */
	skb->len = 0;
	skb->is_clone = 0;
	skb->cloned = 0;

    skb_shinfo(skb)->nr_frags  = 0;
    skb_shinfo(skb)->tso_size = 0;
    skb_shinfo(skb)->tso_segs = 0;
    skb_shinfo(skb)->frag_list = NULL;

	skb_shinfo(skb)->dataref = 1;	

	alloc_skbs ++;


	return skb;

nodata:
	kfree(skb);
nohead:
	InterlockedIncrement(&net_fails);
	return NULL;
}


/*
 *	Slab constructor for a skb head. 
 */ 
//static inline void skb_headerinit(void *p, kmem_cache_t *cache, 
//				  unsigned long flags)
static void skb_headerinit(void *p, void *cache, 
				  unsigned long flags)
{
	struct sk_buff *skb = p;

	UNREFERENCED_PARAMETER(cache);
	UNREFERENCED_PARAMETER(flags);
//	skb->destructor = NULL;
//	skb->pkt_type = PACKET_HOST;	/* Default type */
//	skb->pkt_bridged = 0;		/* Not bridged */
	skb->prev = skb->next = NULL;
	skb->list = NULL;
	skb->sk = NULL;
	skb->stamp.QuadPart=0;	/* No idea about time */
//	skb->ip_summed = 0;
//	skb->security = 0;	/* By default packets are insecure */
//	skb->dst = NULL;
#ifdef CONFIG_IP_FIREWALL
        skb->fwmark = 0;
#endif
	memset(skb->cb, 0, sizeof(skb->cb));
//	skb->priority = 0;
}

/*
 *	Free an skbuff by memory without cleaning the state. 
 */
void kfree_skbmem(struct sk_buff *skb)
{	
//	NbDebugPrint(0, ("kfree_skbmem: skb->cloned = %d, skb_shinfo(skb)->dataref = %d\n", skb->cloned, skb_shinfo(skb)->dataref));
	if (!skb->cloned || (0 == InterlockedDecrement(&(skb_shinfo(skb)->dataref)))) {
		kfree(skb->head);
	}

//	kmem_cache_free(skbuff_head_cache, skb);
	kfree(skb);
	InterlockedDecrement(&net_skbcount);
}

/*
 *	Free an sk_buff. Release anything attached to the buffer. Clean the state.
 */

void __kfree_skb(struct sk_buff *skb)
{
//	if (skb->list)
//	 	NbDebugPrint(0, ("Warning: kfree_skb passed an skb still "
//		       "on a list (from %p).\n", __builtin_return_address(0));

//	if(skb->destructor)
//		skb->destructor(skb);

	if(!skb_cloned(skb)) {
		freed_skbs ++;
	} else {
		clone_freed_skbs ++;
	}
	skb_headerinit(skb, NULL, 0);  /* clean state */
	kfree_skbmem(skb);
}

/*
 *	Duplicate an sk_buff. The new one is not owned by a socket.
 */

struct sk_buff *skb_clone(struct sk_buff *skb, int gfp_mask)
{
	struct sk_buff *n;

	UNREFERENCED_PARAMETER(gfp_mask);

	NbDebugPrint(3, ("skb_clone: skb = %08X\n", skb));
//	n = kmalloc(sizeof(struct sk_buff), gfp_mask);
	n = ExAllocatePoolWithTag(NonPagedPool, sizeof(struct sk_buff), LSMP_PTAG_SKBUFF_HEADER);
	if (!n)
		return NULL;

	memcpy(n, skb, sizeof(*n));
	InterlockedIncrement(&(skb_shinfo(skb)->dataref));
	skb->cloned = 1;
       
	InterlockedIncrement(&net_allocs);
	InterlockedIncrement(&net_skbcount);
//	dst_clone(n->dst);
	n->cloned = 1;
	n->next = n->prev = NULL;
	n->list = NULL;
	n->sk = NULL;
	n->is_clone = 1;
	n->data_len = skb->data_len;
//	atomic_set(&n->users, 1);
//	n->destructor = NULL;
	cloned_skbs ++;

	return n;
}

static void copy_skb_header(struct sk_buff *new, const struct sk_buff *old)
{
        /*
         *      Shift between the two data areas in bytes
         */
        unsigned long offset = new->data - old->data;
 
        new->list       = NULL;
        new->sk         = NULL;
        new->dev        = old->dev;
//        new->real_dev   = old->real_dev;
//        new->priority   = old->priority;
        new->protocol   = old->protocol;
//        new->dst        = dst_clone(old->dst);
#ifdef CONFIG_INET
        new->sp         = secpath_get(old->sp);
#endif
        new->h.raw      = old->h.raw + offset;
        new->nh.raw     = old->nh.raw + offset;
        new->mac.raw    = old->mac.raw + offset;
        memcpy(new->cb, old->cb, sizeof(old->cb));
//        new->local_df   = old->local_df;
        new->pkt_type   = old->pkt_type;
        new->stamp      = old->stamp;
//        new->destructor = NULL;
//        new->security   = old->security;
#ifdef CONFIG_NETFILTER
        new->nfmark     = old->nfmark;
        new->nfcache    = old->nfcache;
        new->nfct       = old->nfct;
        nf_conntrack_get(old->nfct);
        new->nfctinfo   = old->nfctinfo;
#ifdef CONFIG_NETFILTER_DEBUG
        new->nf_debug   = old->nf_debug;
#endif
#ifdef CONFIG_BRIDGE_NETFILTER
        new->nf_bridge  = old->nf_bridge;
        nf_bridge_get(old->nf_bridge);
#endif
#endif
#ifdef CONFIG_NET_SCHED
#ifdef CONFIG_NET_CLS_ACT
        new->tc_verd = old->tc_verd;
#endif
        new->tc_index   = old->tc_index;
#endif
//        atomic_set(&new->users, 1);
        skb_shinfo(new)->tso_size = skb_shinfo(old)->tso_size;
        skb_shinfo(new)->tso_segs = skb_shinfo(old)->tso_segs;
}

/*
 *	This is slower, and copies the whole data area 
 */
 
struct sk_buff *skb_copy(struct sk_buff *skb, int gfp_mask)
{
	struct sk_buff *n;
	unsigned long offset;

	/*
	 *	Allocate the copy buffer
	 */
	
	NbDebugPrint(0, ("skb_copy: skb = %08X\n", skb));
	n=alloc_skb(skb->end - skb->head + skb->data_len, gfp_mask);
	if(n==NULL)
		return NULL;

	/*
	 *	Shift between the two data areas in bytes
	 */
	 
	offset=n->head-skb->head;

	/* Set the data pointer */
	skb_reserve(n,skb->data-skb->head);
	/* Set the tail pointer and length */
	skb_put(n,skb->len);
	/* Copy the bytes */
	memcpy(n->head,skb->head,skb->end-skb->head);
	n->csum = skb->csum;
	n->list=NULL;
	n->sk=NULL;
	n->dev=skb->dev;
//	n->priority=skb->priority;
	n->protocol=skb->protocol;
//	n->dst=dst_clone(skb->dst);
	n->h.raw=skb->h.raw+offset;
	n->nh.raw=skb->nh.raw+offset;
	n->mac.raw=skb->mac.raw+offset;
	memcpy(n->cb, skb->cb, sizeof(skb->cb));
	n->used=skb->used;
	n->is_clone=0;
//	atomic_set(&n->users, 1);
	n->pkt_type=skb->pkt_type;
	n->stamp=skb->stamp;
//	n->destructor = NULL;
//	n->security=skb->security;
#ifdef CONFIG_IP_FIREWALL
        n->fwmark = skb->fwmark;
#endif

	skbs_copy  ++;
		
	return n;
}

struct sk_buff *skb_realloc_headroom(struct sk_buff *skb, int newheadroom)
{
	struct sk_buff *n;
	unsigned long offset;
	int headroom = skb_headroom(skb);

	/*
	 *	Allocate the copy buffer
	 */
 	 
	NbDebugPrint(0, ("skb_realloc_headroom: skb = %08X\n", skb));
	n=alloc_skb(skb->truesize+newheadroom-headroom, 0);
	if(n==NULL)
		return NULL;

	skb_reserve(n,newheadroom);

	/*
	 *	Shift between the two data areas in bytes
	 */
	 
	offset=n->data-skb->data;

	/* Set the tail pointer and length */
	skb_put(n,skb->len);
	/* Copy the bytes */
	memcpy(n->data,skb->data,skb->len);
	n->list=NULL;
	n->sk=NULL;
//	n->priority=skb->priority;
	n->protocol=skb->protocol;
	n->dev=skb->dev;
//	n->dst=dst_clone(skb->dst);
	n->h.raw=skb->h.raw+offset;
	n->nh.raw=skb->nh.raw+offset;
	n->mac.raw=skb->mac.raw+offset;
	memcpy(n->cb, skb->cb, sizeof(skb->cb));
	n->used=skb->used;
	n->is_clone=0;
//	atomic_set(&n->users, 1);
	n->pkt_type=skb->pkt_type;
	n->stamp=skb->stamp;
//	n->destructor = NULL;
//	n->security=skb->security;
#ifdef CONFIG_IP_FIREWALL
        n->fwmark = skb->fwmark;
#endif

	return n;
}

/**
 *      skb_copy_expand -       copy and expand sk_buff
 *      @skb: buffer to copy
 *      @newheadroom: new free bytes at head
 *      @newtailroom: new free bytes at tail
 *      @gfp_mask: allocation priority
 *
 *      Make a copy of both an &sk_buff and its data and while doing so
 *      allocate additional space.
 *
 *      This is used when the caller wishes to modify the data and needs a
 *      private copy of the data to alter as well as more space for new fields.
 *      Returns %NULL on failure or the pointer to the buffer
 *      on success. The returned buffer has a reference count of 1.
 *
 *      You must pass %GFP_ATOMIC as the allocation priority if this function
 *      is called from an interrupt.
 *
 *      BUG ALERT: ip_summed is not copied. Why does this work? Is it used
 *      only by netfilter in the cases when checksum is recalculated? --ANK
 */
struct sk_buff *skb_copy_expand(struct sk_buff *skb,
                                int newheadroom, int newtailroom, int gfp_mask)
{
        /*
         *      Allocate the copy buffer
         */
        struct sk_buff *n = alloc_skb(newheadroom + skb->len + newtailroom,
                                      gfp_mask);
        int head_copy_len, head_copy_off;

        if (!n)
                return NULL;

        skb_reserve(n, newheadroom);

        /* Set the tail pointer and length */
        skb_put(n, skb->len);

		head_copy_len = skb_headroom(skb);		
        head_copy_off = 0;
        if (newheadroom <= head_copy_len)
                head_copy_len = newheadroom;
        else
                head_copy_off = newheadroom - head_copy_len;

        /* Copy the linear header and data. */
        if (skb_copy_bits(skb, -head_copy_len, n->head + head_copy_off,
                          skb->len + head_copy_len))
			NbDebugPrint(0, ("skb_copy_bits error !!\n"));

        copy_skb_header(n, skb);

        return n;
}


/**
*      skb_pad                 -       zero pad the tail of an skb
*      @skb: buffer to pad
*      @pad: space to pad
*
*      Ensure that a buffer is followed by a padding area that is zero
*      filled. Used by network drivers which may DMA or transfer data
*      beyond the buffer end onto the wire.
*
*      May return NULL in out of memory cases.
*/

struct sk_buff *skb_pad(struct sk_buff *skb, int pad)
{
    struct sk_buff *nskb;
    
	NbDebugPrint(3, ("skb_pad: skb = %08X, pad = %d\n", skb, pad));
    /* If the skbuff is non linear tailroom is always zero.. */
    if (skb_tailroom(skb) >= pad) {
            memset(skb->data+skb->len, 0, pad);
            return skb;
    }
    
    nskb = skb_copy_expand(skb, skb_headroom(skb), skb_tailroom(skb) + pad, 0);
    kfree_skb(skb);
    if (nskb)
            memset(nskb->data+nskb->len, 0, pad);
    return nskb;
}       


/* Copy some data bits from skb to kernel buffer. */

int skb_copy_bits(const struct sk_buff *skb, int offset, void *to, int len)
{
        unsigned int i;
		int copy;
        int start = skb_headlen(skb);

        if (offset > (int)skb->len - len)
                goto fault;

        /* Copy header. */
        if ((copy = start - offset) > 0) {
                if (copy > len)
                        copy = len;
                memcpy(to, skb->data + offset, copy);
                if ((len -= copy) == 0)
                        return 0;
                offset += copy;
                to     = (void *)((unsigned char *)to + copy);
        }

        for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
                int end;

                end = start + skb_shinfo(skb)->frags[i].size;
                if ((copy = end - offset) > 0) {
                        unsigned char *vaddr;

                        if (copy > len)
                                copy = len;

//                        vaddr = kmap_skb_frag(&skb_shinfo(skb)->frags[i]);
						vaddr = (unsigned char *)skb_shinfo(skb)->frags[i].page;
                        memcpy(to,
                               vaddr + skb_shinfo(skb)->frags[i].page_offset+
                               offset - start, copy);
//                        kunmap_skb_frag(vaddr);

                        if ((len -= copy) == 0)
                                return 0;
                        offset += copy;
                        to     = (void *)((unsigned char *)to + copy);
                }
                start = end;
        }

        if (skb_shinfo(skb)->frag_list) {
                struct sk_buff *list = skb_shinfo(skb)->frag_list;

                for (; list; list = list->next) {
                        int end;

                        end = start + list->len;
                        if ((copy = end - offset) > 0) {
                                if (copy > len)
                                        copy = len;
                                if (skb_copy_bits(list, offset - start,
                                                  to, copy))
                                        goto fault;
                                if ((len -= copy) == 0)
                                        return 0;
                                offset += copy;
                                to     = (void *)((unsigned char *)to + copy);
                        }
                        start = end;
                }
        }
        if (!len)
                return 0;

fault:
        return -EFAULT;
}
