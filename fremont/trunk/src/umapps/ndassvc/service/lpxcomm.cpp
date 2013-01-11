
		}
		pNdasBlockAce = &l_pNdasBlockAcl->BlockACEs[i - nBACLESkipped];
		pNdasBlockAce->AccessMode |= 
			(pBACLE->AccessMask & BACL_ACCESS_MASK_WRITE) ? NBACE_ACCESS_WRITE : 0;
		pNdasBlockAce->AccessMode |= 
			(pBACLE->AccessMask & BACL_ACCESS_MASK_READ) ? NBACE_ACCESS_READ : 0;
		pNdasBlockAce->BlockStartAddr = pBACLE->ui64StartSector;
		pNdasBlockAce->BlockEndAddr =
			pBACLE->ui64StartSector + pBACLE->ui64SectorCount -1;

		XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION,
			"FillBACL() pNdasBlockAce : %x %I64d ~ %I64d\n",
			pNdasBlockAce->AccessMode,
			pNdasBlockAce->BlockStartAddr,
			pNdasBlockAce->BlockEndAddr);
	}

	l_pNdasBlockAcl->Length = GetBACLSize(nBACLESkipped);
	l_pNdasBlockAcl->BlockACECnt = m_pBACL->ElementCount - nBACLESkipped;

	return TRUE;
}



const NDAS_CONTENT_ENCRYPT NDAS_CONTENT_ENCRYPT_NONE = {
	NDAS_CONTENT_ENCRYPT_METHOD_NONE
};

//////////////////////////////////////////////////////////////////////////
//
// Null Unit Disk Device
//
//////////////////////////////////////////////////////////////////////////

CNdasNullUnitDiskDevice::
CNdasNullUnitDiskDevice(
	CNdasDevicePtr pParentDevice,
	DWORD UnitNo,
	const NDAS_UNITDEVICE_HARDWARE_INFO& UnitDevHardwareInfo,
	NDAS_UNITDEVICE_ERROR Error) :
	CNdasUnitDiskDevice(
		pParentDevice, 
		UnitNo,