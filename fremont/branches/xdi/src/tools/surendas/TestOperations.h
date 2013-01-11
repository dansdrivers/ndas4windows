#ifndef			__TEST_OPERATIONS_H__
#define			__TEST_OPERATIONS_H__

BOOL			TestInitialize(void);
BOOL			IsConnected(PTEST_NETDISK Netdisk);
BOOL			IsValidate(PTEST_NETDISK Netdisk);

BOOL			TestStep2(PTEST_NETDISK Netdisk);
BOOL			TestStep3(PTEST_NETDISK Netdisk);
#endif