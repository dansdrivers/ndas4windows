Readme for surenetdisk.

Last updated: 5:31 PM 7/21/2006 aingoppa

What is surenetdisk?
- surenetdisk is a dialog application to verify all the NDAS devices on same ethernet.

What does surenetdisk do on each netdisk?
- see function UpdateModuleThreadProc(). 
- phase #1 IsValidate() : test connect, login, get disk info and log out
- phase #2 TestStep2() : R/W test
	read fore 1, 5 or 10MB of the disk to a buffer
	write random data on the disk
	read & check the (random) data on the disk
	write the buffer back to the disk
	read & check the (previous) data on the disk
	do same test on center position of the disk
- phase #3 TestStep3() : X Area test
	erase (write 0 data on) 2MB at tail of the disk
	read & check 2MB at tail on the disk

What you should give care to.
- surenetdisk opens 10002 port to catch broadcasting message from NDAS device. Turn off NDAS service or any application that use 10002 port.

=============================================
Build note
=============================================

2.0.0002 5:33 PM 7/21/2006
	Surenetdisk now uses ndascomm.dll

2.0.0001 5:33 PM 7/21/2006
	Accepts NDAS device 2.0

0.1 3:31 AM 1/26/2006
	First check in	