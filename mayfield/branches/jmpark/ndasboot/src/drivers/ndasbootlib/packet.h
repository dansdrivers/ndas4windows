#ifndef __PACKET_H
#define __PACKET_H

#define PACKET_BROADCAST	1
#define PACKET_MULTICAST	2
#define PACKET_OTHERHOST	3

int packet_send_skb(struct sk_buff *skb, UCHAR *dest_addr);
int packet_if_offset(struct sock *sk) ;

#endif