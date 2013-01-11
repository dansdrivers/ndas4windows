#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsdInternalApi.h"




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdInsertPrefix)
#pragma alloc_text(PAGE, XixFsdRemovePrefix)
#pragma alloc_text(PAGE, XixFsdFindPrefix)
#pragma alloc_text(PAGE, XixFsdFCBTableCompare)
#pragma alloc_text(PAGE, XixFsdFCBTableAllocate)
#pragma alloc_text(PAGE, XixFsdFCBTableDeallocate)
#pragma alloc_text(PAGE, XixFsdFCBTableInsertFCB)
#pragma alloc_text(PAGE, XixFsdFCBTableDeleteFCB)
#pragma alloc_text(PAGE, XixFsdLookupFCBTable)
#pragma alloc_text(PAGE, XixFsdGetNextFcb)
#pragma alloc_text(PAGE, XixFsdFullCompareNames) 
#pragma alloc_text(PAGE, XixFsdInsertNameLink) 
#pragma alloc_text(PAGE, XixFsdFindNameLink)
#endif



PXIFS_LCB
XixFsdInsertPrefix (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_FCB Fcb,
	IN PUNICODE_STRING Name,
	IN PXIFS_FCB ParentFcb
)
{
	PXIFS_LCB Lcb;
	PRTL_SPLAY_LINKS *TreeRoot;
	PLIST_ENTRY ListLinks;

	PWCHAR NameBuffer;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdInsertPrefix \n" ));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_FCB( Fcb );

	ASSERT_EXCLUSIVE_FCB( Fcb );
	ASSERT_EXCLUSIVE_FCB( ParentFcb );
	ASSERT( ParentFcb->FCBType == FCB_TYPE_DIR);

	//
	//  It must be the case that an index Fcb is only referenced by a single index.  Now
	//  we walk the child's Lcb queue to insure that if any prefixes have already been
	//  inserted, they all refer to the index Fcb we are linking to.  This is the only way
	//  we can detect directory cross-linkage.
	//

	if (Fcb->FCBType == FCB_TYPE_DIR) {

		for (ListLinks = Fcb->ParentLcbQueue.Flink;
			 ListLinks != &Fcb->ParentLcbQueue;
			 ListLinks = ListLinks->Flink) {

			Lcb = CONTAINING_RECORD( ListLinks, XIFS_LCB, ChildFcbLinks );

			if (Lcb->ParentFcb != ParentFcb) {

				XifsdRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
			}
		}
	}


    //
    //  Allocate space for the Lcb.
    //

	Lcb = XixFsdAllocateLCB(Name->Length);
 
    //
    //  Initialize the name-based file attributes.
    //
    
    Lcb->FileAttributes = 0;
    
    //
    //  Set up the filename in the Lcb.
    //

 
    RtlCopyMemory( Lcb->FileName.Buffer,
                   Name->Buffer,
                   Name->Length );
    
		


	//
	//  Capture the separate cases.
	//

	TreeRoot = &ParentFcb->Root;


    //
    //  Insert the Lcb into the prefix tree.
    //
    
    if (!XixFsdInsertNameLink( IrpContext,
                            TreeRoot,
                            Lcb )) {

        //
        //  This will very rarely occur.
        //

        XixFsdFreeLCB( Lcb );

        Lcb = XixFsdFindNameLink( IrpContext,
                               TreeRoot,
                               Name );

        if (Lcb == NULL) {

            //
            //  Even worse.
            //

            XifsdRaiseStatus( IrpContext, STATUS_DRIVER_INTERNAL_ERROR );
        }


		if(!XifsdCheckFlagBoolean(Lcb->LCBFlags, XIFSD_LCB_STATE_IGNORE_CASE_SET)){
			XifsdRaiseStatus( IrpContext, STATUS_DRIVER_INTERNAL_ERROR );
		}

        return Lcb;
    }

	//
	//  Capture the separate cases.
	//

	TreeRoot = &ParentFcb->IgnoreCaseRoot;
 

    //
    //  Set up the filename in the Lcb.
    //

 	RtlDowncaseUnicodeString(&(Lcb->IgnoreCaseFileName),
						Name,
						FALSE);


	if(!XixFsdInsertNameLinkIgnoreCase( IrpContext,
									TreeRoot,
									Lcb )){
		XifsdRaiseStatus( IrpContext, STATUS_DRIVER_INTERNAL_ERROR );
	}


	XifsdSetFlag(Lcb->LCBFlags, XIFSD_LCB_STATE_IGNORE_CASE_SET);

    //
    //  Link the Fcbs together through the Lcb.
    //

    Lcb->ParentFcb = ParentFcb;
    Lcb->ChildFcb = Fcb;

    InsertHeadList( &ParentFcb->ChildLcbQueue, &Lcb->ParentFcbLinks );
    InsertHeadList( &Fcb->ParentLcbQueue, &Lcb->ChildFcbLinks );

    //
    //  Initialize the reference count.
    //

    Lcb->Reference = 0;
    

	//DbgPrint(" !!!Insert LCB FileName(%wZ) IgnoreFileName(%wZ)  .\n", &Lcb->FileName, &Lcb->IgnoreCaseFileName);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdInsertPrefix \n" ));
    return Lcb;
}


VOID
XixFsdRemovePrefix (
	IN BOOLEAN  CanWait,
	IN PXIFS_LCB Lcb
)
{
    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdRemovePrefix \n" ));
    //
    //  Check inputs.
    //

    ASSERT_LCB( Lcb );

    //
    //  Check the acquisition of the two Fcbs.
    //

    ASSERT_EXCLUSIVE_FCB_OR_VCB( Lcb->ParentFcb );
    ASSERT_EXCLUSIVE_FCB_OR_VCB( Lcb->ChildFcb );

    //
    //  Now remove the linkage and delete the Lcb.
    //
    
    RemoveEntryList( &Lcb->ParentFcbLinks );
    RemoveEntryList( &Lcb->ChildFcbLinks );

 
	if(XifsdCheckFlagBoolean(Lcb->LCBFlags, XIFSD_LCB_STATE_IGNORE_CASE_SET)){
		Lcb->ParentFcb->IgnoreCaseRoot = RtlDelete( &Lcb->IgnoreCaseLinks);
	}

    Lcb->ParentFcb->Root = RtlDelete( &Lcb->Links );
   
	XixFsdFreeLCB(Lcb);
 	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdRemovePrefix \n" ));   
    return;
}






PXIFS_LCB
XixFsdFindPrefix (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN OUT PXIFS_FCB *CurrentFcb,
    IN OUT PUNICODE_STRING RemainingName,
	IN BOOLEAN	bIgnoreCase
    )
{
	UNICODE_STRING LocalRemainingName;
	UNICODE_STRING FinalName;
	
	PXIFS_LCB	NameLink;
	PXIFS_LCB	CurrentLcb = NULL;
	BOOLEAN		Waitable = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdFindPrefix \n" ));   
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_FCB( *CurrentFcb );
	ASSERT_EXCLUSIVE_FCB( *CurrentFcb );


	

	Waitable =  XifsdCheckFlagBoolean(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	try{
		//
		//  Make a local copy of the input strings.
		//

		LocalRemainingName = *RemainingName;

		//
		//  Loop until we find the longest matching prefix.
		//

		while (TRUE) {

			//
			//  If there are no characters left or we are not at an IndexFcb then
			//  return immediately.
			//

			if ((LocalRemainingName.Length == 0) ||
				(XifsSafeNodeType( *CurrentFcb ) != XIFS_NODE_FCB)) {

				try_return(TRUE); 
				// return CurrentLcb;
			}

			if((*CurrentFcb)->FCBType != FCB_TYPE_DIR){
				try_return(TRUE); 
				//return CurrentLcb;

			}

			//
			//  Split off the next component from the name.
			//

			FsRtlDissectName( LocalRemainingName,
								&FinalName,
								&LocalRemainingName);

			//
			//  Check if this name is in the splay tree for this Fcb.
			//


			if(bIgnoreCase){
				NameLink = XixFsdFindNameLinkIgnoreCase( IrpContext,
											&(*CurrentFcb)->IgnoreCaseRoot,
											&FinalName );
			}else{
				NameLink = XixFsdFindNameLink( IrpContext,
											&(*CurrentFcb)->Root,
											&FinalName );
			}



			//
			//  If we didn't find a match then exit.
			//

			if (NameLink == NULL) { 

				break;
			}



			//
			//
			//
			//if ( XifsdCheckFlagBoolean(NameLink->LCBFlags, 
			//	(XIFSD_LCB_STATE_LINK_IS_GONE)) )
			//{
			//	break;
			//}


			CurrentLcb = NameLink;

			//
			//  Update the caller's remaining name string to reflect the fact that we found
			//  a match.
			//

			*RemainingName = LocalRemainingName;

			//
			//  Move down to the next component in the tree.  Acquire without waiting.
			//  If this fails then lock the Fcb to reference this Fcb and then drop
			//  the parent and acquire the child.
			//

			ASSERT( NameLink->ParentFcb == *CurrentFcb );

			if (!XifsdAcquireFcbExclusive( Waitable, NameLink->ChildFcb, FALSE ))  
			{

				//
				//  If we can't wait then raise CANT_WAIT.
				//

				if ( Waitable) {

					XifsdRaiseStatus( IrpContext, STATUS_CANT_WAIT );
				}

				XifsdLockVcb( IrpContext, IrpContext->VCB );
				NameLink->ChildFcb->FCBReference += 1;
				NameLink->Reference += 1;
				XifsdUnlockVcb( IrpContext, IrpContext->VCB );

				XifsdReleaseFcb( IrpContext, *CurrentFcb );
				XifsdAcquireFcbExclusive(  Waitable, NameLink->ChildFcb, FALSE );

				XifsdLockVcb( IrpContext, IrpContext->VCB );
				NameLink->ChildFcb->FCBReference -= 1;
				NameLink->Reference -= 1;
				XifsdUnlockVcb( IrpContext, IrpContext->VCB );

			} else {

				XifsdReleaseFcb( IrpContext, *CurrentFcb );
			}

			*CurrentFcb = NameLink->ChildFcb;
		}
	}finally{
		if(AbnormalTermination()){
			ExRaiseStatus(STATUS_CANT_WAIT);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdFindPrefix \n" ));   
	return CurrentLcb;
}



RTL_GENERIC_COMPARE_RESULTS
XixFsdFCBTableCompare(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID id1,
	IN PVOID id2
)
{
	uint64 Id1, Id2;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdFCBTableCompare \n" ));  

	Id1 = *((uint64 *)id1);
	Id2 = *((uint64 *)id2);

	if(Id1 < Id2){
		return GenericLessThan;
	} else if (Id1 > Id2){
		return GenericGreaterThan;
	} else {
		return GenericEqual;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdFCBTableCompare \n" ));  
}

PVOID
XixFsdFCBTableAllocate(
	IN PRTL_GENERIC_TABLE Table,
	IN uint32 ByteSize
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdFCBTableAllocate \n" ));  

	return (FsRtlAllocatePoolWithTag(PagedPool, ByteSize, TAG_G_TABLE));
}


VOID
XixFsdFCBTableDeallocate(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID Buffer
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdFCBTableDeallocate \n" ));  

	ExFreePool(Buffer);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdFCBTableDeallocate \n" ));  
	return;
}


VOID
XixFsdFCBTableInsertFCB(
	PXIFS_FCB pFCB
)
{
	FCB_TABLE_ENTRY Entry;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdFCBTableInsertFCB \n" ));  

	Entry.FileId = pFCB->LotNumber;
	Entry.pFCB = pFCB;

	RtlInsertElementGenericTable( &pFCB->PtrVCB->FCBTable, &Entry, sizeof(FCB_TABLE_ENTRY), NULL);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdFCBTableInsertFCB \n" ));  
	return;
}


BOOLEAN
XixFsdFCBTableDeleteFCB(
	PXIFS_FCB pFCB
)
{
	FCB_TABLE_ENTRY Entry;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdFCBTableDeleteFCB \n" ));  

	Entry.FileId = pFCB->LotNumber;
	return (RtlDeleteElementGenericTable(&pFCB->PtrVCB->FCBTable, &Entry));
}




PXIFS_FCB
XixFsdLookupFCBTable(
	PXIFS_VCB	VCB,
	uint64		FileId
)
{
	FCB_TABLE_ENTRY		Entry;
	PFCB_TABLE_ENTRY	Hit = NULL;
	PXIFS_FCB			pFCB = NULL;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdLookupFCBTable \n" ));  

	Entry.FileId = FileId;
	Hit = (PFCB_TABLE_ENTRY) RtlLookupElementGenericTable( &VCB->FCBTable, &Entry);
	
	if(Hit != NULL){
		pFCB = Hit->pFCB;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdLookupFCBTable \n" ));  

	return pFCB;
}

PXIFS_FCB
XixFsdGetNextFcb (
    IN PXIFS_VCB Vcb,
    IN PVOID *RestartKey
    )
{
    PXIFS_FCB Fcb;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter XixFsdGetNextFcb  \n" ));  

    Fcb = (PXIFS_FCB) RtlEnumerateGenericTableWithoutSplaying( &Vcb->FCBTable, RestartKey );

    if (Fcb != NULL) {

        Fcb = ((PFCB_TABLE_ENTRY)(Fcb))->pFCB;
    }

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit XixFsdGetNextFcb  \n" ));  
    return Fcb;
}



FSRTL_COMPARISON_RESULT
XixFsdFullCompareNames (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PUNICODE_STRING NameA,
    IN PUNICODE_STRING NameB
    )
{
	ULONG i;
	ULONG MinLength = NameA->Length;
	FSRTL_COMPARISON_RESULT Result = LessThan;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFullCompareNames\n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );

	//
	//  Figure out the minimum of the two lengths
	//

	if (NameA->Length > NameB->Length) {

		MinLength = NameB->Length;
		Result = GreaterThan;

	} else if (NameA->Length == NameB->Length) {

		Result = EqualTo;
	}

	//
	//  Loop through looking at all of the characters in both strings
	//  testing for equalilty, less than, and greater than
	//

	i = (ULONG) RtlCompareMemory( NameA->Buffer, NameB->Buffer, MinLength );

	if (i < MinLength) {

		//
		//  We know the offset of the first character which is different.
		//

		return ((NameA->Buffer[ i / 2 ] < NameB->Buffer[ i / 2 ]) ?
				 LessThan :
				 GreaterThan);
	}

	//
	//  The names match up to the length of the shorter string.
	//  The shorter string lexically appears first.
	//

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFullCompareNames\n"));
	return Result;
}



BOOLEAN
XixFsdInsertNameLink (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIFS_LCB NameLink
)
{
    FSRTL_COMPARISON_RESULT Comparison;
    PXIFS_LCB Node;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdInsertNameLink \n"));
    //
    //  Check inputs.
    //

    ASSERT_IRPCONTEXT( IrpContext );

    RtlInitializeSplayLinks( &NameLink->Links );

    //
    //  If we are the first entry in the tree, just become the root.
    //

    if (*RootNode == NULL) {

        *RootNode = &NameLink->Links;

        return TRUE;
    }

    Node = CONTAINING_RECORD( *RootNode, XIFS_LCB, Links );

    while (TRUE) {

        //
        //  Compare the prefix in the tree with the prefix we want
        //  to insert.
        //

        Comparison = XixFsdFullCompareNames( IrpContext, &Node->FileName, &NameLink->FileName );

        //
        //  If we found the entry, return immediately.
        //

        if (Comparison == EqualTo) { return FALSE; }

        //
        //  If the tree prefix is greater than the new prefix then
        //  we go down the left subtree
        //

        if (Comparison == GreaterThan) {

            //
            //  We want to go down the left subtree, first check to see
            //  if we have a left subtree
            //

            if (RtlLeftChild( &Node->Links ) == NULL) {

                //
                //  there isn't a left child so we insert ourselves as the
                //  new left child
                //

                RtlInsertAsLeftChild( &Node->Links, &NameLink->Links );

                //
                //  and exit the while loop
                //

                break;

            } else {

                //
                //  there is a left child so simply go down that path, and
                //  go back to the top of the loop
                //

                Node = CONTAINING_RECORD( RtlLeftChild( &Node->Links ),
                                          XIFS_LCB,
                                          Links );
            }

        } else {

            //
            //  The tree prefix is either less than or a proper prefix
            //  of the new string.  We treat both cases as less than when
            //  we do insert.  So we want to go down the right subtree,
            //  first check to see if we have a right subtree
            //

            if (RtlRightChild( &Node->Links ) == NULL) {

                //
                //  These isn't a right child so we insert ourselves as the
                //  new right child
                //

                RtlInsertAsRightChild( &Node->Links, &NameLink->Links );

                //
                //  and exit the while loop
                //

                break;

            } else {

                //
                //  there is a right child so simply go down that path, and
                //  go back to the top of the loop
                //

                Node = CONTAINING_RECORD( RtlRightChild( &Node->Links ),
                                          XIFS_LCB,
                                          Links );
            }
        }
    }
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdInsertNameLink \n"));
    return TRUE;
}




PXIFS_LCB
XixFsdFindNameLink (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    )
{
	FSRTL_COMPARISON_RESULT Comparison;
	PXIFS_LCB Node;
	PRTL_SPLAY_LINKS Links;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFindNameLink \n"));

	Links = *RootNode;

	while (Links != NULL) {

		Node = CONTAINING_RECORD( Links, XIFS_LCB, Links );

		//
		//  Compare the prefix in the tree with the full name
		//

		Comparison = XixFsdFullCompareNames( IrpContext, &Node->FileName, Name );

		//
		//  See if they don't match
		//

		if (Comparison == GreaterThan) {

			//
			//  The prefix is greater than the full name
			//  so we go down the left child
			//

			Links = RtlLeftChild( Links );

			//
			//  And continue searching down this tree
			//

		} else if (Comparison == LessThan) {

			//
			//  The prefix is less than the full name
			//  so we go down the right child
			//

			Links = RtlRightChild( Links );

			//
			//  And continue searching down this tree
			//

		} else {

			//
			//  We found it.
			//
			//  Splay the tree and save the new root.
			//

			*RootNode = RtlSplay( Links );

			return Node;
		}
	}

	//
	//  We didn't find the Link.
	//
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFindNameLink \n"));
	return NULL;
}



BOOLEAN
XixFsdInsertNameLinkIgnoreCase (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIFS_LCB NameLink
)
{
    FSRTL_COMPARISON_RESULT Comparison;
    PXIFS_LCB Node;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdInsertNameLink \n"));
    //
    //  Check inputs.
    //

    ASSERT_IRPCONTEXT( IrpContext );

    RtlInitializeSplayLinks( &NameLink->IgnoreCaseLinks );

    //
    //  If we are the first entry in the tree, just become the root.
    //

    if (*RootNode == NULL) {

        *RootNode = &NameLink->IgnoreCaseLinks;

        return TRUE;
    }

    Node = CONTAINING_RECORD( *RootNode, XIFS_LCB, IgnoreCaseLinks );

    while (TRUE) {

        //
        //  Compare the prefix in the tree with the prefix we want
        //  to insert.
        //

        Comparison = XixFsdFullCompareNames( IrpContext, &Node->IgnoreCaseFileName, &NameLink->IgnoreCaseFileName );

        //
        //  If we found the entry, return immediately.
        //

        if (Comparison == EqualTo) { return FALSE; }

        //
        //  If the tree prefix is greater than the new prefix then
        //  we go down the left subtree
        //

        if (Comparison == GreaterThan) {

            //
            //  We want to go down the left subtree, first check to see
            //  if we have a left subtree
            //

            if (RtlLeftChild( &Node->IgnoreCaseLinks ) == NULL) {

                //
                //  there isn't a left child so we insert ourselves as the
                //  new left child
                //

                RtlInsertAsLeftChild( &Node->IgnoreCaseLinks, &NameLink->IgnoreCaseLinks );

                //
                //  and exit the while loop
                //

                break;

            } else {

                //
                //  there is a left child so simply go down that path, and
                //  go back to the top of the loop
                //

                Node = CONTAINING_RECORD( RtlLeftChild( &Node->IgnoreCaseLinks ),
                                          XIFS_LCB,
                                          IgnoreCaseLinks );
            }

        } else {

            //
            //  The tree prefix is either less than or a proper prefix
            //  of the new string.  We treat both cases as less than when
            //  we do insert.  So we want to go down the right subtree,
            //  first check to see if we have a right subtree
            //

            if (RtlRightChild( &Node->IgnoreCaseLinks ) == NULL) {

                //
                //  These isn't a right child so we insert ourselves as the
                //  new right child
                //

                RtlInsertAsRightChild( &Node->IgnoreCaseLinks, &NameLink->IgnoreCaseLinks );

                //
                //  and exit the while loop
                //

                break;

            } else {

                //
                //  there is a right child so simply go down that path, and
                //  go back to the top of the loop
                //

                Node = CONTAINING_RECORD( RtlRightChild( &Node->IgnoreCaseLinks ),
                                          XIFS_LCB,
                                          IgnoreCaseLinks );
            }
        }
    }
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdInsertNameLink \n"));
    return TRUE;
}




PXIFS_LCB
XixFsdFindNameLinkIgnoreCase (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    )
{
	FSRTL_COMPARISON_RESULT Comparison;
	PXIFS_LCB Node;
	PRTL_SPLAY_LINKS Links;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter XixFsdFindNameLink \n"));

	Links = *RootNode;

	while (Links != NULL) {

		Node = CONTAINING_RECORD( Links, XIFS_LCB, IgnoreCaseLinks );

		//
		//  Compare the prefix in the tree with the full name
		//

		Comparison = XixFsdFullCompareNames( IrpContext, &Node->IgnoreCaseFileName, Name );

		//
		//  See if they don't match
		//

		if (Comparison == GreaterThan) {

			//
			//  The prefix is greater than the full name
			//  so we go down the left child
			//

			Links = RtlLeftChild( Links );

			//
			//  And continue searching down this tree
			//

		} else if (Comparison == LessThan) {

			//
			//  The prefix is less than the full name
			//  so we go down the right child
			//

			Links = RtlRightChild( Links );

			//
			//  And continue searching down this tree
			//

		} else {

			//
			//  We found it.
			//
			//  Splay the tree and save the new root.
			//

			*RootNode = RtlSplay( Links );

			return Node;
		}
	}

	//
	//  We didn't find the Link.
	//
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit XixFsdFindNameLink \n"));
	return NULL;
}