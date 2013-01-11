#ifndef _SERVICE_AND_ADMIN_H_
#define _SERVICE_AND_ADMIN_H_

//
// cslee:
// Service Events should be named as Global
// to make them available to multiple sessions,
// with which LDServ and Admin can support multiple sessions
// for Terminal Service and Fast User Switching in Windows XP
//
#define	LDSERV_UPDATE_EVENT_NAME	"Global\\LDSERV_UPDATE_EVENT_V1"
#define LDSERV_ALARM_EVENT_NAME		"Global\\LDSERV_ALARM_EVENT_V1"
#define SZSERVICENAME				"LANSCSIHelper"

#define	NDTAKETURN_UDPPORT			50019

#endif	// _SERVICE_AND_ADMIN_H_
