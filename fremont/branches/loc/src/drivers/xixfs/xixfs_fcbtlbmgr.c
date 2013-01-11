#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_debug.h"
#include "xixfs_global.h"
#include "xixfs_internal.h"




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_FCBTLBInsertPrefix)
#pragma alloc_text(PAGE, xixfs_FCBTLBRemovePrefix)
#pragma alloc_text(PAGE, xixfs_FCBTLBFindPrefix)
#pragma alloc_text(PAGE, xixfs_FCBTLBCompareEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBAllocateEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBDeallocateEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBInsertEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBDeleteEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBLookupEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBGetNextEntry)
#pragma alloc_text(PAGE, xixfs_FCBTLBFullCompareNames) 
#pragma alloc_text(PAGE, xixfs_NLInsertNameLink) 
#pragma alloc_text(PAGE, xixfs_NLFindNameLink)
#endif



PXIXFS_LCB
xixfs_FCBTLBInsertPrefix (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_FCB Fcb,
	IN PUNICODE_STRING Name,
	IN PXIXFS_FCB ParentFcb
)
{
	PXIXFS_LCB Lcb;
	PRTL_SPLAY_LINKS *TreeRoot;
	PLIST_ENTRY ListLinks;

	PWCHAR NameBuffer;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBInsertPrefix \n" ));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_FCB( Fcb );

	ASSERT_EXCLUSIVE_FCB( Fcb );
	ASSERT_EXCLUSIVE_FCB( ParentFcb );
	ASSERT( ParentFcb->XixcoreFcb.FCBType == FCB_TYPE_DIR);

	//
	//  It must be the case that an index Fcb is only referenced by a single index.  Now
	//  we walk the child's Lcb queue to insure that if any prefixes have already been
	//  inserted, they all refer to the index Fcb we are linking to.  This is the only way
	//  we can detect directory cross-linkage.
	//

	if (Fcb->XixcoreFcb.FCBType == FCB_TYPE_DIR) {

		for (ListLinks = Fcb->ParentLcbQueue.Flink;
			 ListLinks != &Fcb->ParentLcbQueue;
			 ListLinks = ListLinks->Flink) {

			Lcb = CONTAINING_RECORD( ListLinks, XIXFS_LCB, ChildFcbLinks );

			if (Lcb->ParentFcb != ParentFcb) {

				XifsdRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR );
			}
		}
	}


    //
    //  Allocate space for the Lcb.
    //

	Lcb = xixfs_AllocateLCB(Name->Length);
 
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
    
    if (!xixfs_NLInsertNameLink( IrpContext,
                            TreeRoot,
                            Lcb )) {

        //
        //  This will very rarely occur.
        //

        xixfs_FreeLCB( Lcb );

        Lcb = xixfs_NLFindNameLink( IrpContext,
                               TreeRoot,
                               Name );

        if (Lcb == NULL) {

            //
            //  Even worse.
            //

            XifsdRaiseStatus( IrpContext, STATUS_DRIVER_INTERNAL_ERROR );
        }


		if(!XIXCORE_TEST_FLAGS(Lcb->LCBFlags, XIFSD_LCB_STATE_IGNORE_CASE_SET)){
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


	if(!xixfs_NLInsertNameLinkIgnoreCase( IrpContext,
									TreeRoot,
									Lcb )){
		XifsdRaiseStatus( IrpContext, STATUS_DRIVER_INTERNAL_ERROR );
	}


	XIXCORE_SET_FLAGS(Lcb->LCBFlags, XIFSD_LCB_STATE_IGNORE_CASE_SET);

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
		("Exit xixfs_FCBTLBInsertPrefix \n" ));
    return Lcb;
}


VOID
xixfs_FCBTLBRemovePrefix (
	IN BOOLEAN  CanWait,
	IN PXIXFS_LCB Lcb
)
{
    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBRemovePrefix \n" ));
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

 
	if(XIXCORE_TEST_FLAGS(Lcb->LCBFlags, XIFSD_LCB_STATE_IGNORE_CASE_SET)){
		Lcb->ParentFcb->IgnoreCaseRoot = RtlDelete( &Lcb->IgnoreCaseLinks);
	}

    Lcb->ParentFcb->Root = RtlDelete( &Lcb->Links );
   
	xixfs_FreeLCB(Lcb);
 	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit xixfs_FCBTLBRemovePrefix \n" ));   
    return;
}






PXIXFS_LCB
xixfs_FCBTLBFindPrefix (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN OUT PXIXFS_FCB *CurrentFcb,
    IN OUT PUNICODE_STRING RemainingName,
	IN BOOLEAN	bIgnoreCase
    )
{
	UNICODE_STRING LocalRemainingName;
	UNICODE_STRING FinalName;
	
	PXIXFS_LCB	NameLink;
	PXIXFS_LCB	CurrentLcb = NULL;
	BOOLEAN		Waitable = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBFindPrefix \n" ));   
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_FCB( *CurrentFcb );
	ASSERT_EXCLUSIVE_FCB( *CurrentFcb );


	

	Waitable =  XIXCORE_TEST_FLAGS(IrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

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

			if((*CurrentFcb)->XixcoreFcb.FCBType != FCB_TYPE_DIR){
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
				NameLink = xixfs_NLFindNameLinkIgnoreCase( IrpContext,
											&(*CurrentFcb)->IgnoreCaseRoot,
											&FinalName );
			}else{
				NameLink = xixfs_NLFindNameLink( IrpContext,
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
			//if ( XIXCORE_TEST_FLAGS(NameLink->LCBFlags, 
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
		("Exit xixfs_FCBTLBFindPrefix \n" ));   
	return CurrentLcb;
}



RTL_GENERIC_COMPARE_RESULTS
xixfs_FCBTLBCompareEntry(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID id1,
	IN PVOID id2
)
{
	uint64 Id1, Id2;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBCompareEntry \n" ));  

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
		("Exit xixfs_FCBTLBCompareEntry \n" ));  
}

PVOID
xixfs_FCBTLBAllocateEntry(
	IN PRTL_GENERIC_TABLE Table,
	IN uint32 ByteSize
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBAllocateEntry \n" ));  

	return (FsRtlAllocatePoolWithTag(PagedPool, ByteSize, TAG_G_TABLE));
}


VOID
xixfs_FCBTLBDeallocateEntry(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID Buffer
)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBDeallocateEntry \n" ));  

	ExFreePool(Buffer);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit xixfs_FCBTLBDeallocateEntry \n" ));  
	return;
}


VOID
xixfs_FCBTLBInsertEntry(
	PXIXFS_FCB pFCB
)
{
	FCB_TABLE_ENTRY Entry;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBInsertEntry \n" ));  

	Entry.FileId = pFCB->XixcoreFcb.LotNumber;
	Entry.pFCB = pFCB;

	RtlInsertElementGenericTable( &pFCB->PtrVCB->FCBTable, &Entry, sizeof(FCB_TABLE_ENTRY), NULL);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit xixfs_FCBTLBInsertEntry \n" ));  
	return;
}


BOOLEAN
xixfs_FCBTLBDeleteEntry(
	PXIXFS_FCB pFCB
)
{
	FCB_TABLE_ENTRY Entry;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBDeleteEntry \n" ));  

	Entry.FileId = pFCB->XixcoreFcb.LotNumber;
	return (RtlDeleteElementGenericTable(&pFCB->PtrVCB->FCBTable, &Entry));
}




PXIXFS_FCB
xixfs_FCBTLBLookupEntry(
	PXIXFS_VCB	VCB,
	uint64		FileId
)
{
	FCB_TABLE_ENTRY		Entry;
	PFCB_TABLE_ENTRY	Hit = NULL;
	PXIXFS_FCB			pFCB = NULL;
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBLookupEntry \n" ));  

	Entry.FileId = FileId;
	Hit = (PFCB_TABLE_ENTRY) RtlLookupElementGenericTable( &VCB->FCBTable, &Entry);
	
	if(Hit != NULL){
		pFCB = Hit->pFCB;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit xixfs_FCBTLBLookupEntry \n" ));  

	return pFCB;
}

PXIXFS_FCB
xixfs_FCBTLBGetNextEntry (
    IN PXIXFS_VCB Vcb,
    IN PVOID *RestartKey
    )
{
    PXIXFS_FCB Fcb;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Enter xixfs_FCBTLBGetNextEntry  \n" ));  

    Fcb = (PXIXFS_FCB) RtlEnumerateGenericTableWithoutSplaying( &Vcb->FCBTable, RestartKey );

    if (Fcb != NULL) {

        Fcb = ((PFCB_TABLE_ENTRY)(Fcb))->pFCB;
    }

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CREATE|DEBUG_TARGET_CLOSE| DEBUG_TARGET_FCB),
		("Exit xixfs_FCBTLBGetNextEntry  \n" ));  
    return Fcb;
}



FSRTL_COMPARISON_RESULT
xixfs_FCBTLBFullCompareNames (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PUNICODE_STRING NameA,
    IN PUNICODE_STRING NameB
    )
{
	ULONG i;
	ULONG MinLength = NameA->Length;
	FSRTL_COMPARISON_RESULT Result = LessThan;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_FCBTLBFullCompareNames\n"));
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
		("Exit xixfs_FCBTLBFullCompareNames\n"));
	return Result;
}



BOOLEAN
xixfs_NLInsertNameLink (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIXFS_LCB NameLink
)
{
    FSRTL_COMPARISON_RESULT Comparison;
    PXIXFS_LCB Node;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_NLInsertNameLink \n"));
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

    Node = CONTAINING_RECORD( *RootNode, XIXFS_LCB, Links );

    while (TRUE) {

        //
        //  Compare the prefix in the tree with the prefix we want
        //  to insert.
        //

        Comparison = xixfs_FCBTLBFullCompareNames( IrpContext, &Node->FileName, &NameLink->FileName );

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
                                          XIXFS_LCB,
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
                                          XIXFS_LCB,
                                          Links );
            }
        }
    }
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_NLInsertNameLink \n"));
    return TRUE;
}




PXIXFS_LCB
xixfs_NLFindNameLink (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    )
{
	FSRTL_COMPARISON_RESULT Comparison;
	PXIXFS_LCB Node;
	PRTL_SPLAY_LINKS Links;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_NLFindNameLink \n"));

	Links = *RootNode;

	while (Links != NULL) {

		Node = CONTAINING_RECORD( Links, XIXFS_LCB, Links );

		//
		//  Compare the prefix in the tree with the full name
		//

		Comparison = xixfs_FCBTLBFullCompareNames( IrpContext, &Node->FileName, Name );

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
		("Exit xixfs_NLFindNameLink \n"));
	return NULL;
}



BOOLEAN
xixfs_NLInsertNameLinkIgnoreCase (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIXFS_LCB NameLink
)
{
    FSRTL_COMPARISON_RESULT Comparison;
    PXIXFS_LCB Node;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_NLInsertNameLink \n"));
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

    Node = CONTAINING_RECORD( *RootNode, XIXFS_LCB, IgnoreCaseLinks );

    while (TRUE) {

        //
        //  Compare the prefix in the tree with the prefix we want
        //  to insert.
        //

        Comparison = xixfs_FCBTLBFullCompareNames( IrpContext, &Node->IgnoreCaseFileName, &NameLink->IgnoreCaseFileName );

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
                                          XIXFS_LCB,
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
                                          XIXFS_LCB,
                                          IgnoreCaseLinks );
            }
        }
    }
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixfs_NLInsertNameLink \n"));
    return TRUE;
}




PXIXFS_LCB
xixfs_NLFindNameLinkIgnoreCase (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    )
{
	FSRTL_COMPARISON_RESULT Comparison;
	PXIXFS_LCB Node;
	PRTL_SPLAY_LINKS Links;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixfs_NLFindNameLink \n"));

	Links = *RootNode;

	while (Links != NULL) {

		Node = CONTAINING_RECORD( Links, XIXFS_LCB, IgnoreCaseLinks );

		//
		//  Compare the prefix in the tree with the full name
		//

		Comparison = xixfs_FCBTLBFullCompareNames( IrpContext, &Node->IgnoreCaseFileName, Name );

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
		("Exit xixfs_NLFindNameLink \n"));
	return NULL;
}