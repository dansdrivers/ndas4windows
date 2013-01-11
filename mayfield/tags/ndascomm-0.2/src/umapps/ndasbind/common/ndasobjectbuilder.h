////////////////////////////////////////////////////////////////////////////
//
// Interface of CDiskObjectBuilder class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NDASOBJECTBUILDER_H_
#define _NDASOBJECTBUILDER_H_

#include "ndasdevice.h"
#include "ndasobject.h"

typedef BOOL (__stdcall *PREFRESH_STATUS)(
	UINT number,
	LPVOID context
	);
typedef PREFRESH_STATUS LPREFRESH_STATUS;

//
// class that builds disk object structure from device list
//
class CDiskObjectBuilder
{
protected:
	//
	// Create a list of disks included in the devices
	//
	static CUnitDiskObjectList BuildDiskObjectList(const CDeviceInfoList listDevice,
		 LPREFRESH_STATUS pFuncRefreshStatus, void *context);
public:
	//
	// Create a disk from the deviceInfo
	// If you do not need to build the hierarchical structure of 
	// disks, use this method to create CUnitDiskObject
	//
	// @param deviceInfo  [In] DeviceInfo that has information about the device.
	// @param nSlotNumber [In] index used to identify a disk in the device in case
	//						  there are more than one disk in the device.
	//						  For most of the case just use the default parameter value, 0.
	//						  WARNING : Don't confuse this slotnumber with the slotnumber
	//									used by service. the slotnumber used by service
	//									is assigned to a disk when the disk is registered
	//									to the service. It is a unique identifier to identify
	//									a disk from a bunch of disks registered to a computer.
	// @return CUnitDiskObject created. 
	//
	static CUnitDiskObjectPtr CreateDiskObject(
				const CDeviceInfoPtr deviceInfo, unsigned _int8 nSlotNumber = 0);
	//
	// Build disk object structure from the list of device
	// If you need to build the hierarchical structure of disks,
	// you should use this method.
	//
	static CDiskObjectPtr Build(const CDeviceInfoList listDevice,
		LPREFRESH_STATUS pFuncRefreshStatus = NULL, void *context = NULL);
};


#endif // _NDASOBJECTBUILDER_H_