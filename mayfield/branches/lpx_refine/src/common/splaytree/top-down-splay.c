/*
                An implementation of top-down splaying
                    D. Sleator <sleator@cs.cmu.edu>
    	                     March 1992

  "Splay trees", or "self-adjusting search trees" are a simple and
  efficient data structure for storing an ordered set.  The data
  structure consists of a binary tree, without parent pointers, and no
  additional fields.  It allows searching, insertion, deletion,
  deletemin, deletemax, splitting, joining, and many other operations,
  all with amortized logarithmic performance.  Since the trees adapt to
  the sequence of requests, their performance on real access patterns is
  typically even better.  Splay trees are described in a number of texts
  and papers [1,2,3,4,5].

  The code here is adapted from simple top-down splay, at the bottom of
  page 669 of [3].  It can be obtained via anonymous ftp from
  spade.pc.cs.cmu.edu in directory /usr/sleator/public.

  The chief modification here is that the splay operation works even if the
  item being splayed is not in the tree, and even if the tree root of the
  tree is NULL.  So the line:

                              t = splay(i, t);

  causes it to search for item with key i in the tree rooted at t.  If it's
  there, it is splayed to the root.  If it isn't there, then the node put
  at the root is the last one before NULL that would have been reached in a
  normal binary search for i.  (It's a neighbor of i in the tree.)  This
  allows many other operations to be easily implemented, as shown below.

  [1] "Fundamentals of data structures in C", Horowitz, Sahni,
       and Anderson-Freed, Computer Science Press, pp 542-547.
  [2] "Data Structures and Their Algorithms", Lewis and Denenberg,
       Harper Collins, 1991, pp 243-251.
  [3] "Self-adjusting Binary Search Trees" Sleator and Tarjan,
       JACM Volume 32, No 3, July 1985, pp 652-686.
  [4] "Data Structure and Algorithm Analysis", Mark Weiss,
       Benjamin Cummins, 1992, pp 119-130.
  [5] "Data Structures, Algorithms, and Performance", Derick Wood,
       Addison-Wesley, 1993, pp 367-375.

  The following code was written by Daniel Sleator, and is released
  in the public domain.
*/

#include <windows.h>
#include "top-down-splay.h"

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
){

	TreeCtx->ItemSize = ItemSize;
	TreeCtx->SizeOfTree = 0;
	TreeCtx->CompareRoutine = CompareRoutine;
	TreeCtx->UnionRoutine = UnionRoutine;
	TreeCtx->SubtractRoutine = SubtractRoutine;
	TreeCtx->AllocateRoutine = AllocateRoutine;
	TreeCtx->FreeRoutine = FreeRoutine;

	return 0;
}


VOID
XCTREE_API
XCFreeTree(
	IN PXCTREE_CTX	TreeCtx,
	IN PXCTREE		Tree
){
	if(Tree == NULL)
		return;

	XCFreeTree(TreeCtx, Tree->Left);
	XCFreeTree(TreeCtx, Tree->Right);

	TreeCtx->SizeOfTree--;
	TreeCtx->FreeRoutine(TreeCtx, Tree);
}

VOID
XCTREE_API
XCSplayDestroy(
	IN OUT PXCTREE_CTX			TreeCtx
){

	if(TreeCtx == NULL) {
		return ;
	}

	memset(TreeCtx, 0, sizeof(XCTREE_CTX));
}

/* Simple top down splay, not requiring i to be in the tree t.  */
/* What it does is described above.                             */
PXCTREE
splay(
	PXCTREE_CTX TreeCtx,
	PUCHAR	Item,
	PXCTREE	Tree
){
    XCTREE N, *l, *r, *y;
    if (Tree == NULL) return Tree;
    N.Left = N.Right = NULL;
    l = r = &N;

    for (;;) {
		if (TreeCtx->CompareRoutine(TreeCtx, Tree->Item, Item) > 0) {
			if (Tree->Left == NULL) break;
			if (TreeCtx->CompareRoutine(TreeCtx, Tree->Left->Item, Item) > 0) {
				y = Tree->Left;                           /* rotate Right */
				Tree->Left = y->Right;
				y->Right = Tree;
				Tree = y;
				if (Tree->Left == NULL) break;
			}
			r->Left = Tree;                               /* link Right */
			r = Tree;
			Tree = Tree->Left;
		} else if (TreeCtx->CompareRoutine(TreeCtx, Tree->Item, Item) < 0) {
			if (Tree->Right == NULL) break;
			if (TreeCtx->CompareRoutine(TreeCtx, Tree->Right->Item, Item) < 0) {
				y = Tree->Right;                          /* rotate Left */
				Tree->Right = y->Left;
				y->Left = Tree;
				Tree = y;
				if (Tree->Right == NULL) break;
			}
			l->Right = Tree;                              /* link Left */
			l = Tree;
			Tree = Tree->Right;
		} else {
			break;
		}
    }

	l->Right = Tree->Left;                                /* assemble */
    r->Left = Tree->Right;
    Tree->Left = N.Right;
    Tree->Right = N.Left;

	return Tree;
}


/* Insert i into the tree t, unless it's already there.    */
/* Return a pointer to the resulting tree.                 */
PXCTREE
XCTREE_API
XCSplayInsert(
	IN PXCTREE_CTX	TreeCtx,
	IN PUCHAR		Item,
	IN PXCTREE		Tree
){
    PXCTREE newNode;
    
    newNode = (PXCTREE) TreeCtx->AllocateRoutine(TreeCtx, SIZEOF_XCTREE_NODE(TreeCtx->ItemSize));
    if (newNode == NULL) {
		return NULL;
    }
    memcpy(newNode->Item, Item, TreeCtx->ItemSize);
    if (Tree == NULL) {
		newNode->Left = newNode->Right = NULL;
		TreeCtx->SizeOfTree = 1;
		return newNode;
    }
    Tree = splay(TreeCtx, Item, Tree);

	//
	//	Duplicate node exists.
	//	Don't add it again.
	//

	if(TreeCtx->CompareRoutine(TreeCtx, Tree->Item, Item) == 0) {

		//
		//	We do not allow duplicate insertion.
		//

		TreeCtx->FreeRoutine(TreeCtx, newNode);

		return NULL;

	} else {

		//
		//	If union is successful,
		//	Don't add it again.
		//
		if(	TreeCtx->UnionRoutine == NULL ||
			TreeCtx->UnionRoutine(TreeCtx, Tree->Item, Item) == FALSE) {

			//
			//	Add it.
			//

			if (TreeCtx->CompareRoutine(TreeCtx, Tree->Item, Item) > 0) {

				newNode->Left = Tree->Left;
				newNode->Right = Tree;
				Tree->Left = NULL;

				TreeCtx->SizeOfTree ++;
				return newNode;

			} else {
				newNode->Right = Tree->Right;
				newNode->Left = Tree;
				Tree->Right = NULL;

				TreeCtx->SizeOfTree ++;
				return newNode;
			}

		} else {
			TreeCtx->FreeRoutine(TreeCtx, newNode);

			return Tree;
		}
	}
}

/* Deletes Item from the tree if it's there.            */
/* Return a pointer to the resulting tree.              */
PXCTREE
XCTREE_API
XCSplayDelete(
	IN PXCTREE_CTX	TreeCtx,
	IN PUCHAR		Item,
	IN PXCTREE		Tree,
	OUT PULONG		Found	OPTIONAL
){

	if(Tree==NULL) {
		if(Found) {
			*Found = FALSE;
		}
		return NULL;
	}


	Tree = splay(TreeCtx, Item, Tree);
    if(TreeCtx->CompareRoutine(TreeCtx, Tree->Item, Item) == 0) { /* found it */

		LONG	split;

		if(Found) {
			*Found = TRUE;
		}
		split = FALSE;

		//
		//	Subtract found item by given item.
		//	If it results in zero value,
		//	delete the tree node.
		//
		if(	TreeCtx->SubtractRoutine == NULL ||
			TreeCtx->SubtractRoutine(TreeCtx, Tree->Item, Item, &split) == TRUE) {

			//
			//	Insert the updated given item
			//	if the given item split the node item.
			//

			if(split == TRUE) {

				Tree = XCSplayInsert(
					TreeCtx,
					Item,
					Tree
					);

			} else {
				PXCTREE x;

				if (Tree->Left == NULL) {
					x = Tree->Right;
				} else {
					x = splay(TreeCtx, Item, Tree->Left);
					x->Right = Tree->Right;
				}

				TreeCtx->SizeOfTree--;
				TreeCtx->FreeRoutine(TreeCtx, Tree);

				Tree = x;
			}

			return Tree;
		}

	} else {
		if(Found) {
			*Found = FALSE;
		}
	}

	return Tree;                         /* It wasn't there */
}


PXCTREE
XCTREE_API
XCSplayLookup(
	IN PXCTREE_CTX TreeCtx,
	IN PUCHAR	Item,
	IN PXCTREE	Tree,
	OUT PULONG	Match
){
	Tree = splay(TreeCtx, Item, Tree);

	if(Match) {
		if(TreeCtx->CompareRoutine(TreeCtx, Tree->Item, Item) == 0) {
			*Match = TRUE;
		} else {
			*Match = FALSE;
		}
	}

	return Tree;
}

/*
void main() {
// A sample use of these functions.  Start with the empty tree,
// insert some stuff into it, and then delete it
    XCTREE * root;
    int i;
    root = NULL;              // the empty tree
    size = 0;
    for (i = 0; i < 1024; i++) {
	root = insert((541*i) & (1023), root);
    }
    for (i = 0; i < 1024; i++) {
	root = delete((541*i) & (1023), root);
    }
    printf("size = %d\n", size);
}
*/