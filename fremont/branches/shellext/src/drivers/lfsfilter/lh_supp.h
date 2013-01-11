#pragma once

#if WINVER < 0x0600

typedef struct _KTRANSACTION KTRANSACTION, *PKTRANSACTION, *RESTRICTED_POINTER PRKTRANSACTION;
typedef struct _KENLISTMENT KENLISTMENT, *PKENLISTMENT, *RESTRICTED_POINTER PRKENLISTMENT;
typedef struct _KRESOURCEMANAGER KRESOURCEMANAGER, *PKRESOURCEMANAGER, *RESTRICTED_POINTER PRKRESOURCEMANAGER;
typedef struct _KTM KTM, *PKTM, *RESTRICTED_POINTER PRKTM;

typedef
NTSTATUS
(*PTM_RM_NOTIFICATION) (
    IN     PKENLISTMENT EnlistmentObject,
    IN     PVOID RMContext,
    IN     PVOID TransactionContext,
    IN     ULONG TransactionNotification,
    IN OUT PLARGE_INTEGER TmVirtualClock,
    IN     ULONG ArgumentLength,
    IN     PVOID Argument
    );

typedef ULONG NOTIFICATION_MASK;
#define TRANSACTION_NOTIFY_MASK                 0x3FFFFFFF
#define TRANSACTION_NOTIFY_PREPREPARE           0x00000001
#define TRANSACTION_NOTIFY_PREPARE              0x00000002
#define TRANSACTION_NOTIFY_COMMIT               0x00000004
#define TRANSACTION_NOTIFY_ROLLBACK             0x00000008
#define TRANSACTION_NOTIFY_PREPREPARE_COMPLETE  0x00000010
#define TRANSACTION_NOTIFY_PREPARE_COMPLETE     0x00000020
#define TRANSACTION_NOTIFY_COMMIT_COMPLETE      0x00000040
#define TRANSACTION_NOTIFY_ROLLBACK_COMPLETE    0x00000080
#define TRANSACTION_NOTIFY_RECOVER              0x00000100
#define TRANSACTION_NOTIFY_SINGLE_PHASE_COMMIT  0x00000200
#define TRANSACTION_NOTIFY_DELEGATE_COMMIT      0x00000400
#define TRANSACTION_NOTIFY_RECOVER_QUERY        0x00000800
#define TRANSACTION_NOTIFY_ENLIST_PREPREPARE    0x00001000
#define TRANSACTION_NOTIFY_LAST_RECOVER         0x00002000
#define TRANSACTION_NOTIFY_INDOUBT              0x00004000
#define TRANSACTION_NOTIFY_PROPAGATE_PULL       0x00008000
#define TRANSACTION_NOTIFY_PROPAGATE_PUSH       0x00010000
#define TRANSACTION_NOTIFY_MARSHAL              0x00020000
#define TRANSACTION_NOTIFY_ENLIST_MASK          0x00040000
#define TRANSACTION_NOTIFY_SAVEPOINT            0x00080000
#define TRANSACTION_NOTIFY_SAVEPOINT_COMPLETE   0x00100000
#define TRANSACTION_NOTIFY_CLEAR_SAVEPOINT      0x00200000
#define TRANSACTION_NOTIFY_CLEAR_ALL_SAVEPOINTS 0x00400000
#define TRANSACTION_NOTIFY_ROLLBACK_SAVEPOINT   0x00800000
#define TRANSACTION_NOTIFY_RM_DISCONNECTED      0x01000000
#define TRANSACTION_NOTIFY_TM_ONLINE            0x02000000
#define TRANSACTION_NOTIFY_COMMIT_REQUEST       0x04000000
#define TRANSACTION_NOTIFY_PROMOTE              0x08000000
#define TRANSACTION_NOTIFY_PROMOTE_NEW          0x10000000
#define TRANSACTION_NOTIFY_REQUEST_OUTCOME      0x20000000

//////////////////////////////////////////////////////////////////////////
//
// Excerpts from winnt.h in WDK 6000
//
//////////////////////////////////////////////////////////////////////////

//
// KTM Tm object rights
//
#define TRANSACTIONMANAGER_QUERY_INFORMATION     ( 0x0001 )
#define TRANSACTIONMANAGER_SET_INFORMATION       ( 0x0002 )
#define TRANSACTIONMANAGER_RECOVER               ( 0x0004 )
#define TRANSACTIONMANAGER_RENAME                ( 0x0008 )
#define TRANSACTIONMANAGER_CREATE_RM             ( 0x0010 )

// The following right is intended for DTC's use only; it will be
// deprecated, and no one else should take a dependency on it.
#define TRANSACTIONMANAGER_BIND_TRANSACTION      ( 0x0020 )

//
// Generic mappings for transaction manager rights.
//

#define TRANSACTIONMANAGER_GENERIC_READ            (STANDARD_RIGHTS_READ            |\
	TRANSACTIONMANAGER_QUERY_INFORMATION)

#define TRANSACTIONMANAGER_GENERIC_WRITE           (STANDARD_RIGHTS_WRITE           |\
	TRANSACTIONMANAGER_SET_INFORMATION     |\
	TRANSACTIONMANAGER_RECOVER             |\
	TRANSACTIONMANAGER_RENAME              |\
	TRANSACTIONMANAGER_CREATE_RM)

#define TRANSACTIONMANAGER_GENERIC_EXECUTE         (STANDARD_RIGHTS_EXECUTE)

#define TRANSACTIONMANAGER_ALL_ACCESS              (STANDARD_RIGHTS_REQUIRED        |\
	TRANSACTIONMANAGER_GENERIC_READ        |\
	TRANSACTIONMANAGER_GENERIC_WRITE       |\
	TRANSACTIONMANAGER_GENERIC_EXECUTE     |\
	TRANSACTIONMANAGER_BIND_TRANSACTION)

//
// KTM resource manager object rights.
//
#define RESOURCEMANAGER_QUERY_INFORMATION     ( 0x0001 )
#define RESOURCEMANAGER_SET_INFORMATION       ( 0x0002 )
#define RESOURCEMANAGER_RECOVER               ( 0x0004 )
#define RESOURCEMANAGER_ENLIST                ( 0x0008 )
#define RESOURCEMANAGER_GET_NOTIFICATION      ( 0x0010 )
#define RESOURCEMANAGER_REGISTER_PROTOCOL     ( 0x0020 )
#define RESOURCEMANAGER_COMPLETE_PROPAGATION  ( 0x0040 )

//
// Generic mappings for resource manager rights.
//
#define RESOURCEMANAGER_GENERIC_READ        (STANDARD_RIGHTS_READ                 |\
                                             RESOURCEMANAGER_QUERY_INFORMATION    |\
                                             SYNCHRONIZE)

#define RESOURCEMANAGER_GENERIC_WRITE       (STANDARD_RIGHTS_WRITE                |\
                                             RESOURCEMANAGER_SET_INFORMATION      |\
                                             RESOURCEMANAGER_RECOVER              |\
                                             RESOURCEMANAGER_ENLIST               |\
                                             RESOURCEMANAGER_GET_NOTIFICATION     |\
                                             RESOURCEMANAGER_REGISTER_PROTOCOL    |\
                                             RESOURCEMANAGER_COMPLETE_PROPAGATION |\
                                             SYNCHRONIZE)

#define RESOURCEMANAGER_GENERIC_EXECUTE     (STANDARD_RIGHTS_EXECUTE              |\
                                             RESOURCEMANAGER_RECOVER              |\
                                             RESOURCEMANAGER_ENLIST               |\
                                             RESOURCEMANAGER_GET_NOTIFICATION     |\
                                             RESOURCEMANAGER_COMPLETE_PROPAGATION |\
                                             SYNCHRONIZE)

#define RESOURCEMANAGER_ALL_ACCESS          (STANDARD_RIGHTS_REQUIRED             |\
                                             RESOURCEMANAGER_GENERIC_READ         |\
                                             RESOURCEMANAGER_GENERIC_WRITE        |\
                                             RESOURCEMANAGER_GENERIC_EXECUTE)

typedef struct _TXN_PARAMETER_BLOCK {

	USHORT Length;              // sizeof( TXN_PARAMETER_BLOCK )
	USHORT TxFsContext;         // this is mini version of the requested file,
	PVOID  TransactionObject;   // referenced pointer to KTRANSACTION

} TXN_PARAMETER_BLOCK, *PTXN_PARAMETER_BLOCK;

//////////////////////////////////////////////////////////////////////////
//
// excerpts from ktmtypes.h
//
//////////////////////////////////////////////////////////////////////////

//
// Define the TransactionManager option values
//

#define TRANSACTION_MANAGER_VOLATILE              0x00000001
#define TRANSACTION_MANAGER_COMMIT_DEFAULT        0x00000000
#define TRANSACTION_MANAGER_COMMIT_SYSTEM_VOLUME  0x00000002
#define TRANSACTION_MANAGER_COMMIT_SYSTEM_HIVES   0x00000004
#define TRANSACTION_MANAGER_COMMIT_LOWEST         0x00000008
#define TRANSACTION_MANAGER_CORRUPT_FOR_RECOVERY  0x00000010
#define TRANSACTION_MANAGER_CORRUPT_FOR_PROGRESS  0x00000020
#define TRANSACTION_MANAGER_MAXIMUM_OPTION        0x0000003F

//
// Define the ResourceManager option values
//

#define RESOURCE_MANAGER_VOLATILE            0x00000001
#define RESOURCE_MANAGER_COMMUNICATION       0x00000002
#define RESOURCE_MANAGER_MAXIMUM_OPTION      0x00000003

//
// KTM enlistment object rights.
//
#define ENLISTMENT_QUERY_INFORMATION     ( 0x0001 )
#define ENLISTMENT_SET_INFORMATION       ( 0x0002 )
#define ENLISTMENT_RECOVER               ( 0x0004 )
#define ENLISTMENT_SUBORDINATE_RIGHTS    ( 0x0008 )
#define ENLISTMENT_SUPERIOR_RIGHTS       ( 0x0010 )

//
// Generic mappings for enlistment rights.
//
#define ENLISTMENT_GENERIC_READ        (STANDARD_RIGHTS_READ           |\
                                        ENLISTMENT_QUERY_INFORMATION)

#define ENLISTMENT_GENERIC_WRITE       (STANDARD_RIGHTS_WRITE          |\
                                        ENLISTMENT_SET_INFORMATION     |\
                                        ENLISTMENT_RECOVER             |\
                                        ENLISTMENT_SUBORDINATE_RIGHTS  |\
                                        ENLISTMENT_SUPERIOR_RIGHTS)

#define ENLISTMENT_GENERIC_EXECUTE     (STANDARD_RIGHTS_EXECUTE        |\
                                        ENLISTMENT_RECOVER             |\
                                        ENLISTMENT_SUBORDINATE_RIGHTS  |\
                                        ENLISTMENT_SUPERIOR_RIGHTS)

#define ENLISTMENT_ALL_ACCESS          (STANDARD_RIGHTS_REQUIRED       |\
                                        ENLISTMENT_GENERIC_READ        |\
                                        ENLISTMENT_GENERIC_WRITE       |\
                                        ENLISTMENT_GENERIC_EXECUTE)


#endif

