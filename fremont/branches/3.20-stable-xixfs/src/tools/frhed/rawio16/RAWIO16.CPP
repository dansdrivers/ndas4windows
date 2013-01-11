#include <windows.h>
#include <memory.h>

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"

//
struct SectorInfo
{
	 BYTE bDrive;
	 WORD wCylinder;
	 BYTE bHead;
	 BYTE bSector;
	 BYTE bCount;
};

typedef SectorInfo far *LPSectorInfo;


/*
	Data structure used for EI13 functions
*/

struct BlockInfo
{
	BYTE drive;
	DWORD blockAddressLo;
	DWORD blockAddressHi;
	WORD count;
};

typedef BlockInfo far *LPBlockInfo;


struct DiskAddressPacket
{
	BYTE size;
	BYTE reserved;
	WORD count;
	DWORD buffer;
	DWORD startLo;
	DWORD startHi;
};

struct DriveParameters
{
	WORD size;
	WORD flags;
	DWORD cylinders;
	DWORD heads;
	DWORD sectors;
	DWORD sectorsLo;
	DWORD sectorsHi;
	WORD bytesPerSector;
//---v2.0+ ---
	DWORD EDDptr;
//---v3.0 ---
	WORD signature;
	BYTE v3size;
	BYTE reserved[3];
	BYTE bus[4];
	BYTE interface[8];
	BYTE interfacePath[8];
	BYTE devicePath[8];
	BYTE reserved2;
	BYTE checksum;
};

struct ExtDriveInfo
{
	BYTE drive;
	DWORD cylinders;
	DWORD heads;
	DWORD sectors;
	DWORD sectorsLo;
	DWORD sectorsHi;
	WORD bytesPerSector;
};


void QTRACE(char* pszFormat, ... )
{
	static BOOL bFirst = TRUE;
	
	FILE* fp = fopen("trace.log",bFirst ? "w" : "a");
	if( fp != 0 )
	{
		bFirst = FALSE;
		va_list args;
		va_start( args, pszFormat );
		vfprintf( fp, pszFormat, args );
		fclose(fp);
	}
	
}



/*
	End new types
*/


#ifdef __cplusplus
extern "C" {            /* Assume C declarations for C++ */
#endif  /* __cplusplus */
	DWORD FAR PASCAL __export ResetDisk(LPSectorInfo s);

	DWORD FAR PASCAL __export ReadPhysicalSector (LPSectorInfo s,  LPBYTE lpBuffer, DWORD  cbBuffSize);

	DWORD FAR PASCAL __export WritePhysicalSector (LPSectorInfo s, LPBYTE lpBuffer, DWORD  cbBuffSize);

	DWORD FAR PASCAL __export ReadDiskGeometry (LPSectorInfo s);

/*
	Extended Int 13 Functions
*/
	DWORD FAR PASCAL __export EI13GetDriveParameters(ExtDriveInfo* b);
	DWORD FAR PASCAL __export EI13ReadSector (LPBlockInfo b, LPBYTE lpBuffer, DWORD bufferSize);
	DWORD FAR PASCAL __export EI13WriteSector(LPBlockInfo b, LPBYTE lpBuffer, DWORD bufferSize);
/*
	End new functions
*/


#ifdef __cplusplus
}
#endif  /* __cplusplus */


//--------------------------------------------------------------------
	// Code in the 16-bit DLL

	// Converts two BYTEs into a WORD.  This is useful for working with
	// a RMCS, but was not provided in WINDOWS.H.

	#define MAKEWORD(low, high) \
				  ((WORD)((((WORD)(high)) << 8) | ((BYTE)(low))))

	#define SECTOR_SIZE 512       // Size, in bytes, of a disk sector
	#define CARRY_FLAG  0x0001


	typedef BYTE FAR *LPBYTE;

	typedef struct tagRMCS {

		DWORD edi, esi, ebp, RESERVED, ebx, edx, ecx, eax;
		WORD  wFlags, es, ds, fs, gs, ip, cs, sp, ss;

	} RMCS, FAR* LPRMCS;

	BOOL FAR PASCAL SimulateRM_Int (BYTE bIntNum, LPRMCS lpCallStruct);

	void FAR PASCAL BuildRMCS (LPRMCS lpCallStruct);
	BOOL FAR PASCAL __export ReadPhysicalSector1 (BYTE   bDrive,
																 LPBYTE lpBuffer,
																 DWORD  cbBuffSize);

	/*--------------------------------------------------------------------
	  ReadPhysicalSector1()

	  Calls DPMI to call the BIOS Int 13h Read Track function to read the
	  first physical sector of a physical drive. This function is used to
	  read partition tables, for example.

	  Parameters
		  bDrive
			  The Int 13h device unit,
				  0x00 for floppy drive 0
				  0x00 for floppy drive 1
				  0x80 for physical hard disk 0
				  0x81 for physical hard disk 1
				  etc.

		  lpBuffer
			  Pointer to a buffer that receives the sector data.  The buffer
			  must be at least SECTOR_SIZE bytes long.

		  cbBuffSize
			  Actual size of lpBuffer.

	  Return Value
		  Returns TRUE if the first sector was read into the buffer pointed
		  to by lpBuffer, or FALSE otherwise.

	  Assumptions
		  Assumes that sectors are at least SECTOR_SIZE bytes long.

	--------------------------------------------------------------------*/
	DWORD FAR PASCAL __export ResetDisk(LPSectorInfo s)
	{
		BOOL   fResult;
		RMCS   callStruct;

		/*
		  Validate params:
			  bDrive should be int 13h device unit -- let the BIOS validate
				  this parameter -- user could have a special controller with
				  its own BIOS.
			  lpBuffer must not be NULL
			  cbBuffSize must be large enough to hold a single sector
		*/

		/*
		  Initialize the real-mode call structure and set all values needed
		  to read the first sector of the specified physical drive.
		*/

		BuildRMCS (&callStruct);

		callStruct.eax = MAKEWORD(0x00, 0x00);        // BIOS Reset disk system
		callStruct.edx = MAKEWORD(s->bDrive, 0x00);       // Head 0, Drive #

		/*
			Call Int 13h BIOS Read Track and check both the DPMI call
			itself and the BIOS Read Track function result for success.  If
			successful, copy the sector data retrieved by the BIOS into the
			caller's buffer.
		*/

		fResult = SimulateRM_Int (0x13, &callStruct);

		return fResult;
	}


	DWORD FAR PASCAL __export ReadPhysicalSector (LPSectorInfo s,
																	LPBYTE lpBuffer,
																	DWORD  cbBuffSize)
	{
		BOOL   fResult;
		RMCS   callStruct;
		DWORD  gdaBuffer;     // Return value of GlobalDosAlloc().
		LPBYTE RMlpBuffer;    // Real-mode buffer pointer
		LPBYTE PMlpBuffer;    // Protected-mode buffer pointer

		BYTE CylinderLo;
		BYTE CylinderHi;

		/*
		  Validate params:
			  bDrive should be int 13h device unit -- let the BIOS validate
				  this parameter -- user could have a special controller with
				  its own BIOS.
			  lpBuffer must not be NULL
			  cbBuffSize must be large enough to hold a single sector
		*/

		if (lpBuffer == NULL || cbBuffSize < (DWORD)(SECTOR_SIZE * s->bCount))
			return FALSE;

		/*
		  Alelocate the buffer that the Int 13h function will put th sector
		  data into. As this function uses DPMI to call the real-mode BIOS, it
		  must allocate the buffer below 1 MB, and must use a real-mode
		  paragraph-segment address.

		  After the memory has been allocated, create real-mode and
		  protected-mode pointers to the buffer. The real-mode pointer
		  will be used by the BIOS, and the protected-mode pointer will be
		  used by this function because it resides in a Windows 16-bit DLL,
		  which runs in protected mode.
		*/

		gdaBuffer = GlobalDosAlloc (cbBuffSize);

		if (!gdaBuffer)
			return FALSE;

		RMlpBuffer = (LPBYTE)MAKELONG(0, HIWORD(gdaBuffer));
		PMlpBuffer = (LPBYTE)MAKELONG(0, LOWORD(gdaBuffer));

		/*
		  Initialize the real-mode call structure and set all values needed
		  to read the first sector of the specified physical drive.
		*/

		BuildRMCS (&callStruct);

		//
		// John's code
		//
		CylinderLo = (BYTE) (s->wCylinder & 0xFF);
		CylinderHi = (BYTE) ((s->wCylinder >> 8) & 0xFF);
		// bits 6 and 7 of sector are the high bits of cylinder
		s->bSector = (s->bSector & 0x3F) | ((CylinderHi << 6) & 0xC0);

		callStruct.eax = MAKEWORD(s->bCount, 0x02);        // BIOS read, 1 sector
		callStruct.ecx = MAKEWORD(s->bSector, CylinderLo); // Sector 1, Cylinder 0
		callStruct.edx = MAKEWORD(s->bDrive, s->bHead);       // Head 0, Drive #
		callStruct.ebx = LOWORD(RMlpBuffer);    // Offset of sector buffer
		callStruct.es  = HIWORD(RMlpBuffer);    // Segment of sector buffer

		/*
			Call Int 13h BIOS Read Track and check both the DPMI call
			itself and the BIOS Read Track function result for success.  If
			successful, copy the sector data retrieved by the BIOS into the
			caller's buffer.
		*/

		fResult = SimulateRM_Int (0x13, &callStruct);
		if(fResult) // DPMI success
		{
			if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
			{
				// try again?
				// do a disk reset
				ResetDisk(s);

				fResult = SimulateRM_Int (0x13, &callStruct); // call int
				if(fResult) // DPMI success
				{
					if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
					{
						fResult = FALSE;
					}
				}
			}
		}

		if(fResult)
		{
			// copy the data back into the buffer
			_fmemcpy(lpBuffer, PMlpBuffer, (size_t)cbBuffSize);
		}

		// Free the sector data buffer this function allocated
		GlobalDosFree (LOWORD(gdaBuffer));

		return fResult;
	}

	DWORD FAR PASCAL __export WritePhysicalSector (LPSectorInfo s,
																	  LPBYTE lpBuffer,
																	  DWORD  cbBuffSize)
	{
		BOOL   fResult;
		RMCS   callStruct;
		DWORD  gdaBuffer;     // Return value of GlobalDosAlloc().
		LPBYTE RMlpBuffer;    // Real-mode buffer pointer
		LPBYTE PMlpBuffer;    // Protected-mode buffer pointer

		BYTE CylinderLo;
		BYTE CylinderHi;

		/*
		  Validate params:
			  bDrive should be int 13h device unit -- let the BIOS validate
				  this parameter -- user could have a special controller with
				  its own BIOS.
			  lpBuffer must not be NULL
			  cbBuffSize must be large enough to hold a single sector
		*/

		if (lpBuffer == NULL || cbBuffSize < (DWORD)(SECTOR_SIZE * s->bCount))
			return FALSE;

		/*
		  Alelocate the buffer that the Int 13h function will put th sector
		  data into. As this function uses DPMI to call the real-mode BIOS, it
		  must allocate the buffer below 1 MB, and must use a real-mode
		  paragraph-segment address.

		  After the memory has been allocated, create real-mode and
		  protected-mode pointers to the buffer. The real-mode pointer
		  will be used by the BIOS, and the protected-mode pointer will be
		  used by this function because it resides in a Windows 16-bit DLL,
		  which runs in protected mode.
		*/

		gdaBuffer = GlobalDosAlloc (cbBuffSize);

		if (!gdaBuffer)
			return FALSE;

		RMlpBuffer = (LPBYTE)MAKELONG(0, HIWORD(gdaBuffer));
		PMlpBuffer = (LPBYTE)MAKELONG(0, LOWORD(gdaBuffer));

		/*
		  Initialize the real-mode call structure and set all values needed
		  to read the first sector of the specified physical drive.
		*/

		BuildRMCS (&callStruct);

		//
		// John's code
		//
		CylinderLo = (BYTE)(s->wCylinder & 0xFF);
		CylinderHi = (BYTE)((s->wCylinder >> 8) & 0xFF);
		// bits 6 and 7 of sector are the high bits of cylinder
		s->bSector = (s->bSector & 0x3F) | ((CylinderHi << 6) & 0xC0);

		callStruct.eax = MAKEWORD(s->bCount, 0x03);        // BIOS write, 1 sector
		callStruct.ecx = MAKEWORD(s->bSector, CylinderLo); // Sector 1, Cylinder 0
		callStruct.edx = MAKEWORD(s->bDrive, s->bHead);       // Head 0, Drive #
		callStruct.ebx = LOWORD(RMlpBuffer);    // Offset of sector buffer
		callStruct.es  = HIWORD(RMlpBuffer);    // Segment of sector buffer

		/*
			Call Int 13h BIOS Read Track and check both the DPMI call
			itself and the BIOS Read Track function result for success.  If
			successful, copy the sector data retrieved by the BIOS into the
			caller's buffer.
		*/

		// copy the data into the buffer

		_fmemcpy(PMlpBuffer, lpBuffer, (size_t)cbBuffSize);

		// call the interrupt
		fResult = SimulateRM_Int (0x13, &callStruct);
		if(fResult) // DPMI success
		{
			if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
			{
				// try again?
				// do a disk reset
				ResetDisk(s);

				fResult = SimulateRM_Int (0x13, &callStruct); // call int
				if(fResult) // DPMI success
				{
					if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
					{
						fResult = FALSE;
					}
				}
			}
		}

		// Free the sector data buffer this function allocated
		GlobalDosFree (LOWORD(gdaBuffer));

		return fResult;
	}

	DWORD FAR PASCAL __export ReadDiskGeometry (LPSectorInfo s)
		{
		BOOL   fResult;
		RMCS   callStruct;

		BYTE CylinderLo;
		BYTE CylinderHi;

		/*
		  Validate params:
			  bDrive should be int 13h device unit -- let the BIOS validate
				  this parameter -- user could have a special controller with
				  its own BIOS.
			  lpBuffer must not be NULL
			  cbBuffSize must be large enough to hold a single sector
		*/

		/*
		  Initialize the real-mode call structure and set all values needed
		  to read the first sector of the specified physical drive.
		*/

		BuildRMCS (&callStruct);

		callStruct.eax = MAKEWORD(0x00, 0x08);        // BIOS read, 1 sector
		callStruct.ecx = MAKEWORD(0x00, 0x00); // Sector 1, Cylinder 0
		callStruct.edx = MAKEWORD(s->bDrive, 0x00);       // Head 0, Drive #
		callStruct.ebx = 0;//LOWORD(RMlpBuffer);    // Offset of sector buffer
		callStruct.es  = 0;//HIWORD(RMlpBuffer);    // Segment of sector buffer

		/*
			Call Int 13h BIOS Read Track and check both the DPMI call
			itself and the BIOS Read Track function result for success.  If
			successful, copy the sector data retrieved by the BIOS into the
			caller's buffer.
		*/

		fResult = SimulateRM_Int (0x13, &callStruct);
		if(fResult)
		{
		if(callStruct.wFlags & CARRY_FLAG)
			{
				ResetDisk(s);
				// try again
				fResult = SimulateRM_Int (0x13, &callStruct);
				if (fResult)
				{
				if(callStruct.wFlags & CARRY_FLAG)
					{
					fResult = FALSE;
					}
				}
			}
		}

		if(fResult)
		{
			// copy the data into the structure
			s->bHead     = HIBYTE(callStruct.edx);
			CylinderLo   = HIBYTE(callStruct.ecx);
			CylinderHi   = LOBYTE(callStruct.ecx) >> 6;
			s->wCylinder = MAKEWORD(CylinderLo, CylinderHi);
			s->bSector   = (LOBYTE(callStruct.ecx) & 0x3F);
		}

		return fResult;
	}

	/*--------------------------------------------------------------------
	  SimulateRM_Int()

	  Allows protected mode software to execute real mode interrupts such
	  as calls to DOS TSRs, DOS device drivers, etc.

	  This function implements the "Simulate Real Mode Interrupt" function
	  of the DPMI specification v0.9.

	  Parameters
		  bIntNum
			  Number of the interrupt to simulate

		  lpCallStruct
			  Call structure that contains params (register values) for
			  bIntNum.

	  Return Value
		  SimulateRM_Int returns TRUE if it succeeded or FALSE if it
		  failed.

	  Comments
		  lpCallStruct is a protected-mode selector:offset address, not a
		  real-mode segment:offset address.

	--------------------------------------------------------------------*/

	BOOL FAR PASCAL SimulateRM_Int (BYTE bIntNum, LPRMCS lpCallStruct)

		{
		BOOL fRetVal = FALSE;      // Assume failure

		_asm {
				push di

				mov  ax, 0300h         //; DPMI Simulate Real Mode Int
				mov  bl, bIntNum       //; Number of the interrupt to simulate
				mov  bh, 01h           //; Bit 0 = 1; all other bits must be 0
				xor  cx, cx            //; No words to copy
				les  di, lpCallStruct
				int  31h                   //; Call DPMI
				jc   END1                  //; CF set if error occurred
				mov  fRetVal, TRUE
			END1:
				pop di
			  }
		return (fRetVal);
		}

	/*--------------------------------------------------------------------
	  BuildRMCS()

	  Initializes a real mode call structure to contain zeros in all its
	  members.

	  Parameters:
		  lpCallStruct
			  Points to a real mode call structure

	  Comments:
		  lpCallStruct is a protected-mode selector:offset address, not a
		  real-mode segment:offset address.

	--------------------------------------------------------------------*/

	void FAR PASCAL BuildRMCS (LPRMCS lpCallStruct)
		{
		_fmemset (lpCallStruct, 0, sizeof (RMCS));
		}


/*
	 This is the implementation of the Extended Int 0x13 functions
	 Written by John Newbigin
*/



DWORD FAR PASCAL __export EI13GetDriveParameters(ExtDriveInfo* b)
{
	BOOL   fResult;
	RMCS   callStruct;

	DWORD  dpBuffer;     // Return value of GlobalDosAlloc().
	LPBYTE RMlpdpBuffer;    // Real-mode buffer pointer
	LPBYTE PMlpdpBuffer;    // Protected-mode buffer pointer

	DriveParameters *dpptr;


	BuildRMCS (&callStruct);

	callStruct.eax = MAKEWORD(0, 0x41);        // IBM/MS INT 13 Extensions - INSTALLATION CHECK
	callStruct.ebx = 0x55AA;
	callStruct.edx = MAKEWORD(b->drive, 0);

	fResult = SimulateRM_Int (0x13, &callStruct);
	if(fResult) // DPMI success
	{
		if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
		{
			fResult = FALSE;
		}
		if(LOWORD(callStruct.ebx) != 0xAA55)
		{
			fResult = FALSE;
		}
	}
	if(fResult)
	{
		// get the version info
		// version = HIBYTE(LOWORD(callStruct.eax));

		// query the specified drive
		dpBuffer = GlobalDosAlloc (sizeof(DriveParameters));
		if (!dpBuffer)
		{
			return FALSE;
		}

		RMlpdpBuffer = (LPBYTE)MAKELONG(0, HIWORD(dpBuffer));
		PMlpdpBuffer = (LPBYTE)MAKELONG(0, LOWORD(dpBuffer));

		dpptr=(DriveParameters *)PMlpdpBuffer;

		dpptr->size = sizeof(DriveParameters);

		BuildRMCS (&callStruct);

		callStruct.eax = MAKEWORD(0, 0x48); // GET DRIVE PARAMETERS
		callStruct.edx = MAKEWORD(b->drive, 0);

		callStruct.ds  = HIWORD(RMlpdpBuffer);
		callStruct.esi  = LOWORD(RMlpdpBuffer);

		fResult = SimulateRM_Int (0x13, &callStruct);
		if(fResult) // DPMI success
		{
			if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
			{
				fResult = FALSE;
			}
		}
		if(fResult)
		{
			b->cylinders = dpptr->cylinders;
			b->heads = dpptr->heads;
			b->sectors = dpptr->sectors;
			b->sectorsLo = dpptr->sectorsLo;
			b->sectorsHi = dpptr->sectorsHi;
			b->bytesPerSector = dpptr->bytesPerSector;         
			
	QTRACE("cylinders=%ld\n", (long) dpptr->cylinders );
	QTRACE("heads=%ld\n", (long) dpptr->heads );
	QTRACE("sectors=%ld\n", (long) dpptr->sectors );
	QTRACE("sectorsLo=%ld\n", (long) dpptr->sectorsLo );
	QTRACE("sectorsHi=%ld\n", (long) dpptr->sectorsHi );
	QTRACE("bytesPerSector=%ld\n", (long)dpptr->bytesPerSector );
		}
		GlobalDosFree (LOWORD(dpBuffer));
	}
	return fResult;
}

DWORD FAR PASCAL __export EI13ReadSector (LPBlockInfo b, LPBYTE lpBuffer, DWORD bufferSize)
{
	BOOL   fResult;
	RMCS   callStruct;

	DWORD  gdaBuffer;     // Return value of GlobalDosAlloc().
	LPBYTE RMlpBuffer;    // Real-mode buffer pointer
	LPBYTE PMlpBuffer;    // Protected-mode buffer pointer

	DWORD  dapBuffer;     // Return value of GlobalDosAlloc().
	LPBYTE RMlpdapBuffer;    // Real-mode buffer pointer
	LPBYTE PMlpdapBuffer;    // Protected-mode buffer pointer

//	struct DiskAddressPacket dap;
	DiskAddressPacket FAR *lpdap;

	if (lpBuffer == NULL || bufferSize < (SECTOR_SIZE * b->count))
	{
		return FALSE;
	}


		gdaBuffer = GlobalDosAlloc (bufferSize);

		if (!gdaBuffer)
			return FALSE;

		RMlpBuffer = (LPBYTE)MAKELONG(0, HIWORD(gdaBuffer));
		PMlpBuffer = (LPBYTE)MAKELONG(0, LOWORD(gdaBuffer));

		dapBuffer = GlobalDosAlloc(sizeof(DiskAddressPacket));
		if(!dapBuffer)
		{
			GlobalDosFree (LOWORD(gdaBuffer));
			return FALSE;
		}

		RMlpdapBuffer = (LPBYTE)MAKELONG(0, HIWORD(dapBuffer));
		PMlpdapBuffer = (LPBYTE)MAKELONG(0, LOWORD(dapBuffer));

		/*
		  Initialize the real-mode call structure and set all values needed
		  to read the first sector of the specified physical drive.
		*/

		// fill in dap
		lpdap = (DiskAddressPacket *)PMlpdapBuffer;

		lpdap->size   = sizeof(DiskAddressPacket);
		lpdap->count  = b->count;
		lpdap->buffer = (DWORD)RMlpBuffer;
		lpdap->startLo  = b->blockAddressLo;
		lpdap->startHi  = b->blockAddressHi;

		BuildRMCS (&callStruct);


		callStruct.eax = MAKEWORD(0, 0x42);        // BIOS read
		callStruct.edx = MAKEWORD(b->drive, 0);       // Drive #

		callStruct.ds  = HIWORD(RMlpdapBuffer);
		callStruct.esi  = LOWORD(RMlpdapBuffer);

		/*
			Call Int 13h BIOS Read Track and check both the DPMI call
			itself and the BIOS Read Track function result for success.  If
			successful, copy the sector data retrieved by the BIOS into the
			caller's buffer.
		*/

		fResult = SimulateRM_Int (0x13, &callStruct);
		if(fResult) // DPMI success
		{
			if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
			{
				fResult = FALSE;
			}
		}

		if(fResult)
		{
			// copy the data back into the buffer
			_fmemcpy(lpBuffer, PMlpBuffer, (size_t)bufferSize);
		}

		// Free the sector data buffer this function allocated
		GlobalDosFree (LOWORD(gdaBuffer));
		GlobalDosFree (LOWORD(dapBuffer));

		return fResult;
	}

DWORD FAR PASCAL __export EI13WriteSector (LPBlockInfo b, LPBYTE lpBuffer, DWORD bufferSize)
{
	BOOL   fResult;
	RMCS   callStruct;

	DWORD  gdaBuffer;     // Return value of GlobalDosAlloc().
	LPBYTE RMlpBuffer;    // Real-mode buffer pointer
	LPBYTE PMlpBuffer;    // Protected-mode buffer pointer

	DWORD  dapBuffer;     // Return value of GlobalDosAlloc().
	LPBYTE RMlpdapBuffer;    // Real-mode buffer pointer
	LPBYTE PMlpdapBuffer;    // Protected-mode buffer pointer

	DiskAddressPacket FAR *lpdap;

	if (lpBuffer == NULL || bufferSize < (SECTOR_SIZE * b->count))
	{
		return FALSE;
	}


		gdaBuffer = GlobalDosAlloc (bufferSize);

		if (!gdaBuffer)
			return FALSE;

		RMlpBuffer = (LPBYTE)MAKELONG(0, HIWORD(gdaBuffer));
		PMlpBuffer = (LPBYTE)MAKELONG(0, LOWORD(gdaBuffer));

		dapBuffer = GlobalDosAlloc(sizeof(DiskAddressPacket));
		if(!dapBuffer)
		{
			GlobalDosFree (LOWORD(gdaBuffer));
			return FALSE;
		}

		RMlpdapBuffer = (LPBYTE)MAKELONG(0, HIWORD(dapBuffer));
		PMlpdapBuffer = (LPBYTE)MAKELONG(0, LOWORD(dapBuffer));

		/*
		  Initialize the real-mode call structure and set all values needed
		  to read the first sector of the specified physical drive.
		*/

		// fill in dap
		lpdap = (DiskAddressPacket *)PMlpdapBuffer;

		lpdap->size   = sizeof(DiskAddressPacket);
		lpdap->count  = b->count;
		lpdap->buffer = (DWORD)RMlpBuffer;
		lpdap->startLo  = b->blockAddressLo;
		lpdap->startHi  = b->blockAddressHi;

		// copy the buffer into rm buffer
		_fmemcpy(PMlpBuffer, lpBuffer, (size_t)bufferSize);

		BuildRMCS (&callStruct);


		callStruct.eax = MAKEWORD(0, 0x43);        // BIOS write, no verify
		callStruct.edx = MAKEWORD(b->drive, 0);       // Drive #

		callStruct.ds  = HIWORD(RMlpdapBuffer);
		callStruct.esi  = LOWORD(RMlpdapBuffer);

		/*
			Call Int 13h BIOS Read Track and check both the DPMI call
			itself and the BIOS Read Track function result for success.  If
			successful, copy the sector data retrieved by the BIOS into the
			caller's buffer.
		*/

		fResult = SimulateRM_Int (0x13, &callStruct);
		if(fResult) // DPMI success
		{
			if(callStruct.wFlags & CARRY_FLAG) // check carry flag for error
			{
				fResult = FALSE;
			}
		}


		// Free the sector data buffer this function allocated
		GlobalDosFree (LOWORD(gdaBuffer));
		GlobalDosFree (LOWORD(dapBuffer));

		return fResult;
	}


      // prototype for function in .obj file from the thunk script
      extern "C" BOOL FAR PASCAL __export thk_ThunkConnect16(LPSTR lpDll16,
                                      LPSTR lpDll32,
                                      WORD  hInst,
                                      DWORD dwReason);


#include "stdio.h"
#include "stdlib.h"

extern "C" BOOL FAR PASCAL __export DllEntryPoint(DWORD dwReason,
                                 WORD  hInst,
                                 WORD  wDS,
                                 WORD  wHeapSize,
                                 DWORD dwReserved1,
                                 WORD  wReserved2)
{
 if (!thk_ThunkConnect16("RAWIO16.DLL",
                         "RAWIO32.DLL",
                         hInst,
                         dwReason))
 {
    return FALSE;
 }
 return TRUE;
} 
