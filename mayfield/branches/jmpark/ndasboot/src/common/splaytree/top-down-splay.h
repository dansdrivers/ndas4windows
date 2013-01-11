#ifndef _TOPDOWN_SPLAYTREE_H_
#define _TOPDOWN_SPLAYTREE_H_


#ifdef __cplusplus
extern "C"
{
#endif

#define XCTREE_API		__stdcall

//
//	Callback routines
//
typedef struct _XCTREE_CTX XCTREE_CTX, *PXCTREE_CTX;

typedef
LONG
//XCTREE_API
(*PXCTREE_COMPARE_ROUTINE) (
		PXCTREE_CTX TreeCtx,
		PUCHAR  FirstStruct,
		PUCHAR  SecondStruct);

typedef
LONG
//XCTREE_API
(*PXCTREE_UNION_ROUTINE) (
		PXCTREE_CTX TreeCtx,
		PUCHAR  FirstStruct,
		PUCHAR  SecondStruct);

typedef
LONG
//XCTREE_API
(*PXCTREE_SUBTRACT_ROUTINE) (
		PXCTREE_CTX TreeCtx,
		PUCHAR  FirstStruct,
		PUCHAR  SecondStruct,
		PLONG	Split);

typedef
PVOID
//XCTREE_API
(*PXCTREE_ALLOCATE_ROUTINE) (
		PXCTREE_CTX TreeCtx,
		LONG_PTR	ByteSize);

typedef
VOID
//XCTREE_API
(*PXCTREE_FREE_ROUTINE) (
	PXCTREE_CTX TreeCtx,
	PVOID  Buffer);


//
//	Structures
//

#define SIZEOF_XCTREE_NODE(ITEMSIZE)	(FIELD_OFFSET(XCTREE, Item) + (ITEMSIZE))

typedef struct _XCTREE XCTREE, *PXCTREE;
struct _XCTREE {
	PXCTREE Left, Right;

	UCHAR Item[1];
};


typedef struct _XCTREE_CTX {

	ULONG		ItemSize;
	ULONG		SizeOfTree;			/* number of nodes in the tree */


	//
	//	Callbacks
	//

	PXCTREE_COMPARE_ROUTINE		CompareRoutine;
	PXCTREE_UNION_ROUTINE		UnionRoutine;
	PXCTREE_SUBTRACT_ROUTINE	SubtractRoutine;
	PXCTREE_ALLOCATE_ROUTINE	AllocateRoutine;
	PXCTREE_FREE_ROUTINE		FreeRoutine;

} XCTREE_CTX, *PXCTREE_CTX;




//
//	exported functions
//

LONG
XCTREE_API
XCSplayInit(
	IN OUT PXCTREE_CTX			TreeCtx,
	IN ULONG					ItemSize,
	IN PXCTREE_COMPARE_ROUTINE	CompareRoutine,
	IN PXCTREE_UNION_ROUTINE	UnionRoutine OPTIONAL,
	IN PXCTREE_SUBTRACT_ROUTINE	SubtractRoutine OPTIONAL,
	IN PXCTREE_ALLOCATE_ROUTINE	AllocateRoutine,
	IN PXCTREE_FREE_ROUTINE		FreeRoutine
);


VOID
XCTREE_API
XCSplayDestroy(
	IN PXCTREE_CTX			TreeCtx
);

VOID
XCTREE_API
XCFreeTree(
	IN PXCTREE_CTX	TreeCtx,
	IN PXCTREE		Tree
);

PXCTREE
XCTREE_API
XCSplayInsert(
	IN PXCTREE_CTX	TreeCtx,
	IN PUCHAR		Item,
	IN PXCTREE		Tree,
	OUT PCHAR		Inserted
);

PXCTREE
XCTREE_API
XCSplayDelete(
	IN PXCTREE_CTX	TreeCtx,
	IN PUCHAR		item,
	IN PXCTREE		tree,
	OUT PULONG		Deleted	OPTIONAL
);

PXCTREE
XCTREE_API
XCSplayLookup(
	IN PXCTREE_CTX TreeCtx,
	IN PUCHAR	Item,
	IN PXCTREE	Tree,
	OUT PULONG	Match
);

#ifdef __cplusplus
}
#endif

#endif