/*
 *	Definitions for the 'struct sk_buff' memory handlers.
 *
 *	Authors:
 *		Alan Cox, <gw4pts@gw4pts.ampr.org>
 *		Florian La Roche, <rzsfl@rz.uni-sb.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
 
#ifndef __SKBUFF_H
#define __SKBUFF_H

#define CHECKSUM_NONE 0
#define CHECKSUM_HW 1
#define CHECKSUM_UNNECESSARY 2

#define __u8	UCHAR
#define __u16	USHORT
#define	__u32	ULONG

struct sk_buff_head {
	struct sk_buff	* next;
	struct sk_buff	* prev;
	__u32		qlen;		/* Must be same length as a pointer
					   for using debugging */
};

/* To allow 64K frame to be packed as single skb without frag_list */
#define MAX_SKB_FRAGS (65536/PAGE_SIZE + 2)

typedef struct skb_frag_struct skb_frag_t;

struct skb_frag_struct {
	struct page *page;
	__u16 page_offset;
	__u16 size;
};

/* This data is invariant across clones and lives at
* the end of the header data, ie. at skb->end.
*/
struct skb_shared_info {
	unsigned int   dataref;
	unsigned int    nr_frags;
	unsigned short  tso_size;
	unsigned short  tso_segs;
	struct sk_buff  *frag_list;
	skb_frag_t      frags[MAX_SKB_FRAGS];
};


struct sk_buff {
	struct sk_buff	* next;			/* Next buffer in list 				*/
	struct sk_buff	* prev;			/* Previous buffer in list 			*/
	struct sk_buff_head * list;		/* List we are on				*/
	struct sock	*sk;			/* Socket we are owned by 			*/
	ULARGE_INTEGER	stamp;			/* Time we arrived				*/
	struct device	*dev;			/* Device we arrived on/are leaving by		*/

	/* Transport layer header */
	union
	{
		struct tcphdr	*th;
		struct udphdr	*uh;
		struct icmphdr	*icmph;
		struct igmphdr	*igmph;
		struct iphdr	*ipiph;
		struct spxhdr	*spxh;
		unsigned char	*raw;
	} h;

	/* Network layer header */
	union
	{
		struct lpxhdr	*lpxh;
		struct iphdr	*iph;
		struct ipv6hdr	*ipv6h;
		struct arphdr	*arph;
		struct ipxhdr	*ipxh;
		unsigned char	*raw;
	} nh;
  
	/* Link layer header */
	union 
	{	
	  	struct ethhdr	*ethernet;
	  	unsigned char 	*raw;
	} mac;

	struct  dst_entry *dst;

	char		cb[48];	 

	unsigned int 	len,			/* Length of actual data			*/
					data_len,
					mac_len;

	unsigned int	csum;			/* Checksum 					*/
	volatile char 	used;			/* Data moved to user and not MSG_PEEK		*/
	unsigned char	is_clone,		/* We are a clone				*/
			cloned, 		/* head may be cloned (check refcnt to be sure). */
  			pkt_type,		/* Packet class					*/
  			pkt_bridged,		/* Tracker for bridging 			*/
  			ip_summed;		/* Driver fed us an IP checksum			*/
//	__u32		priority;		/* Packet queueing priority			*/
//	ULONG	users;			/* User count - see datagram.c,tcp.c 		*/
	unsigned short	protocol;		/* Packet protocol from driver. 		*/
//	unsigned short	security;		/* Security level of packet			*/
	unsigned int	truesize;		/* Buffer size 					*/

	unsigned char	*head;			/* Head of buffer 				*/
	unsigned char	*data;			/* Data head pointer				*/
	unsigned char	*tail;			/* Tail pointer					*/
	unsigned char 	*end;			/* End pointer					*/
//	void 		(*destructor)(struct sk_buff *);	/* Destruct function		*/
};

/* These are just the default values. This is run time configurable.
 * FIXME: Probably the config option should go away. -- erics
 */
#ifdef CONFIG_SKB_LARGE
#define SK_WMEM_MAX	65535
#define SK_RMEM_MAX	65535
#else
#define SK_WMEM_MAX	32767
#define SK_RMEM_MAX	32767
#endif

extern void			__kfree_skb(struct sk_buff *skb);
extern void			skb_queue_head_init(struct sk_buff_head *list);
extern void			skb_queue_head(struct sk_buff_head *list,struct sk_buff *buf);
extern void			skb_queue_tail(struct sk_buff_head *list,struct sk_buff *buf);
extern struct sk_buff *		skb_dequeue(struct sk_buff_head *list);
extern void 			skb_insert(struct sk_buff *old,struct sk_buff *newsk);
extern void			skb_append(struct sk_buff *old,struct sk_buff *newsk);
extern void			skb_unlink(struct sk_buff *buf);
extern __u32			skb_queue_len(struct sk_buff_head *list);
extern struct sk_buff *		skb_peek_copy(struct sk_buff_head *list);
extern struct sk_buff *		alloc_skb(unsigned int size, int priority);
extern struct sk_buff *		dev_alloc_skb(unsigned int size);
extern void			kfree_skbmem(struct sk_buff *skb);
extern struct sk_buff *		skb_clone(struct sk_buff *skb, int priority);
extern struct sk_buff *		skb_copy(struct sk_buff *skb, int priority);
extern struct sk_buff *		skb_realloc_headroom(struct sk_buff *skb, int newheadroom);
#define dev_kfree_skb(a)	kfree_skb(a)
extern unsigned char *		skb_put(struct sk_buff *skb, unsigned int len);
extern unsigned char *		skb_push(struct sk_buff *skb, unsigned int len);
extern unsigned char *		skb_pull(struct sk_buff *skb, unsigned int len);
extern int			skb_headroom(struct sk_buff *skb);
extern int			skb_tailroom(struct sk_buff *skb);
extern void			skb_reserve(struct sk_buff *skb, unsigned int len);
extern void 			skb_trim(struct sk_buff *skb, unsigned int len);

extern struct sk_buff *skb_copy_expand(struct sk_buff *skb,int newheadroom, int newtailroom,int priority);
extern struct sk_buff *         skb_pad(struct sk_buff *skb, int pad);
extern void	skb_over_panic(struct sk_buff *skb, int len, void *here);
extern void	skb_under_panic(struct sk_buff *skb, int len, void *here);

void show_net_buffers(void);
/* Internal */

#define skb_shinfo(SKB)         ((struct skb_shared_info *)((SKB)->end))

extern __inline int skb_queue_empty(struct sk_buff_head *list)
{
	return (list->next == (struct sk_buff *) list);
}

extern __inline void kfree_skb(struct sk_buff *skb)
{
//	if (atomic_dec_and_test(&skb->users))
		__kfree_skb(skb);
}

/* Use this if you didn't touch the skb state [for fast switching] */
extern __inline void kfree_skb_fast(struct sk_buff *skb)
{
//	if (atomic_dec_and_test(&skb->users))
		kfree_skbmem(skb);	
}

extern __inline int skb_cloned(struct sk_buff *skb)
{
	return skb->cloned && skb_shinfo(skb)->dataref != 1;
}

//extern inline int __inline skb_shared(struct sk_buff *skb)
//{
//	return (atomic_read(&skb->users) != 1);
//}

/*
 *	Copy shared buffers into a new sk_buff. We effectively do COW on
 *	packets to handle cases where we have a local reader and forward
 *	and a couple of other messy ones. The normal one is tcpdumping
 *	a packet thats being forwarded.
 */
 
extern __inline struct  sk_buff *skb_unshare(struct sk_buff *skb, int pri)
{
	struct sk_buff *nskb;
	if(!skb_cloned(skb))
		return skb;
	nskb=skb_copy(skb, pri);
	kfree_skb(skb);		/* Free our shared copy */
	return nskb;
}

/*
 *	Peek an sk_buff. Unlike most other operations you _MUST_
 *	be careful with this one. A peek leaves the buffer on the
 *	list and someone else may run off with it. For an interrupt
 *	type system cli() peek the buffer copy the data and sti();
 */
 
extern __inline struct sk_buff *skb_peek(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->next;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

extern __inline struct sk_buff *skb_peek_tail(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *)list_)->prev;
	if (list == (struct sk_buff *)list_)
		list = NULL;
	return list;
}

/*
 *	Return the length of an sk_buff queue
 */
 
extern __inline __u32 skb_queue_len(struct sk_buff_head *list_)
{
	return(list_->qlen);
}

extern __inline void skb_queue_head_init(struct sk_buff_head *list)
{
	list->prev = (struct sk_buff *)list;
	list->next = (struct sk_buff *)list;
	list->qlen = 0;
}

/*
 *	Insert an sk_buff at the start of a list.
 *
 *	The "__skb_xxxx()" functions are the non-atomic ones that
 *	can only be called with interrupts disabled.
 */

extern __inline void __skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	prev = (struct sk_buff *)list;
	next = prev->next;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}


KSPIN_LOCK skb_queue_lock;

extern __inline void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	KIRQL   Irql;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	__skb_queue_head(list, newsk);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
}

/*
 *	Insert an sk_buff at the end of a list.
 */

extern __inline void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	newsk->list = list;
	list->qlen++;
	next = (struct sk_buff *)list;
	prev = next->prev;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

extern __inline void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	KIRQL Irql;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	__skb_queue_tail(list, newsk);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
}

/*
 *	Remove an sk_buff from a list.
 */

extern __inline struct sk_buff *__skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *next, *prev, *result;

	prev = (struct sk_buff *) list;
	next = prev->next;
	result = NULL;
	if (next != prev) {
		result = next;
		next = next->next;
		list->qlen--;
		next->prev = prev;
		prev->next = next;
		result->next = NULL;
		result->prev = NULL;
		result->list = NULL;
	}
	return result;
}

extern __inline struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	KIRQL Irql;
	struct sk_buff *result;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	result = __skb_dequeue(list);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
	return result;
}

/*
 *	Insert a packet on a list.
 */

extern __inline void __skb_insert(struct sk_buff *newsk,
	struct sk_buff * prev, struct sk_buff *next,
	struct sk_buff_head * list)
{
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
	newsk->list = list;
	list->qlen++;
}

/*
 *	Place a packet before a given packet in a list
 */
extern __inline void skb_insert(struct sk_buff *old, struct sk_buff *newsk)
{
	KIRQL Irql;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	__skb_insert(newsk, old->prev, old, old->list);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
}

/*
 *	Place a packet after a given packet in a list.
 */

extern __inline void __skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	__skb_insert(newsk, old, old->next, old->list);
}

extern __inline void skb_append(struct sk_buff *old, struct sk_buff *newsk)
{
	KIRQL Irql;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	__skb_append(old, newsk);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
}

/*
 * remove sk_buff from list. _Must_ be called atomically, and with
 * the list known..
 */
extern __inline void __skb_unlink(struct sk_buff *skb, struct sk_buff_head *list)
{
	struct sk_buff * next, * prev;

	list->qlen--;
	next = skb->next;
	prev = skb->prev;
	skb->next = NULL;
	skb->prev = NULL;
	skb->list = NULL;
	next->prev = prev;
	prev->next = next;
}

/*
 *	Remove an sk_buff from its list. Works even without knowing the list it
 *	is sitting on, which can be handy at times. It also means that THE LIST
 *	MUST EXIST when you unlink. Thus a list must have its contents unlinked
 *	_FIRST_.
 */

extern __inline void skb_unlink(struct sk_buff *skb)
{
	KIRQL Irql;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	if(skb->list)
		__skb_unlink(skb, skb->list);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
}

extern __inline unsigned int skb_headlen(const struct sk_buff *skb)
{
	return skb->len - skb->data_len;
}

/* XXX: more streamlined implementation */
extern __inline struct sk_buff *__skb_dequeue_tail(struct sk_buff_head *list)
{
	struct sk_buff *skb = skb_peek_tail(list); 
	if (skb)
		__skb_unlink(skb, list);
	return skb;
}

extern __inline struct sk_buff *skb_dequeue_tail(struct sk_buff_head *list)
{
	KIRQL Irql;
	struct sk_buff *result;

	KeAcquireSpinLock(&skb_queue_lock, &Irql);
	result = __skb_dequeue_tail(list);
	KeReleaseSpinLock(&skb_queue_lock, Irql);
	return result;
}

/*
 *	Add data to an sk_buff
 */
 
extern __inline unsigned char *__skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	skb->tail+=len;
	skb->len+=len;
	return tmp;
}

extern __inline unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	skb->tail+=len;
	skb->len+=len;
	if(skb->tail>skb->end)
	{
//		__label__ here; 
//		skb_over_panic(skb, len, &&here); 
//here:		;
	}
	return tmp;
}

extern __inline unsigned char *__skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	return skb->data;
}

extern __inline unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	if(skb->data<skb->head)
	{
//		__label__ here;
//		skb_under_panic(skb, len, &&here);
//here: 		;
	}
	return skb->data;
}

extern __inline char *__skb_pull(struct sk_buff *skb, unsigned int len)
{
	skb->len-=len;
	return 	skb->data+=len;
}

extern __inline unsigned char * skb_pull(struct sk_buff *skb, unsigned int len)
{	
	if (len > skb->len)
		return NULL;
	return __skb_pull(skb,len);
}

extern __inline int skb_headroom(struct sk_buff *skb)
{
	return skb->data-skb->head;
}

extern __inline int skb_tailroom(struct sk_buff *skb)
{
	return skb->end-skb->tail;
}

extern __inline void skb_reserve(struct sk_buff *skb, unsigned int len)
{
	skb->data+=len;
	skb->tail+=len;
}

extern __inline void __skb_trim(struct sk_buff *skb, unsigned int len)
{
	skb->len = len;
	skb->tail = skb->data+len;
}

extern __inline void skb_trim(struct sk_buff *skb, unsigned int len)
{
	if (skb->len > len) {
		__skb_trim(skb, len);
	}
}

extern __inline void skb_orphan(struct sk_buff *skb)
{
//	if (skb->destructor)
//		skb->destructor(skb);
//	skb->destructor = NULL;
	skb->sk = NULL;
}

extern __inline void skb_queue_purge(struct sk_buff_head *list)
{
	struct sk_buff *skb;
	while ((skb=skb_dequeue(list))!=NULL)
		kfree_skb(skb);
}

extern __inline struct sk_buff *dev_alloc_skb(unsigned int length)
{
	struct sk_buff *skb;

	skb = alloc_skb(length+16, 0);
	if (skb)
		skb_reserve(skb,16);
	return skb;
}

extern __inline struct sk_buff *
skb_cow(struct sk_buff *skb, unsigned int headroom)
{
	headroom = (headroom+15)&~15;

	if ((unsigned)skb_headroom(skb) < headroom || skb_cloned(skb)) {
		struct sk_buff *skb2 = skb_realloc_headroom(skb, headroom);
		kfree_skb(skb);
		skb = skb2;
	}
	return skb;
}

extern __inline struct sk_buff *skb_padto(struct sk_buff *skb, unsigned int len)
{
    unsigned int size = skb->len;
    if (size >= len)
            return skb;
    return skb_pad(skb, len-size);
}

extern __inline struct sk_buff *		skb_recv_datagram(struct sock *sk,unsigned flags,int noblock, int *err) {return 0;};
extern __inline unsigned int		datagram_poll(struct file *file, struct socket *sock, struct poll_table_struct *wait) {return 0;};
extern __inline int			skb_copy_datagram(struct sk_buff *from, int offset, char *to,int size) {return 0;};
extern __inline int			skb_copy_datagram_iovec(struct sk_buff *from, int offset, struct iovec *to,int size) {return 0;};
extern __inline void			skb_free_datagram(struct sock * sk, struct sk_buff *skb) {};
extern int             skb_copy_bits(const struct sk_buff *skb, int offset, void *to, int len);


extern void skbuff_init(void);

#endif	/* __SKBUFF_H */
