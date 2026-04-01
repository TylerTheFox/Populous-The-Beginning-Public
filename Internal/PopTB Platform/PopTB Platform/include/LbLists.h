//***************************************************************************
// LbLists.h : Linked list
//***************************************************************************
#ifndef _LBLISTS_H
#define _LBLISTS_H

#include    "Pop3Types.h"


//***************************************************************************
// LbList_Node and LbList : The basis for doubly linked lists
//***************************************************************************
struct TbListNode;


typedef struct TbList
{
    LB_PUBLIC_MEMBERS
    void WatcomIsCrapBecauseIHaveToAddThisFunction()	{};
    LB_PRIVATE_MEMBERS
    struct TbListNode *lpFirst, *lpNull, *lpLast;
} TbList;

typedef struct TbListNode
{
    LB_PUBLIC_MEMBERS
    void WatcomIsCrapBecauseIHaveToAddThisFunction()	{};
    LB_PRIVATE_MEMBERS
    struct TbListNode *lpNext, *lpPrev;
#ifdef _LB_DEBUG
    TbList *lpList;
#endif
} TbListNode;






#define LbList_BeforeFirst(lpList)	(TbListNode*)&(lpList)->lpFirst
#define LbList_AfterLast(lpList)	(TbListNode*)&(lpList)->lpLast
#define LbList_For(list, pVar)		for((pVar)=(list).lpFirst;(pVar)->lpNext;(pVar)=(pVar)->lpNext)

/*#ifdef __cplusplus
template<class T>
class TbListT
{
protected:
	T *lpFirst, *lpNull, *lpLast;

public:
	// Empty the list without releasing each node - use RemoveAll for that.
	void Null()
	{
		lpFirst = (T*)&lpNull;
		lpNull = NULL;
		lpLast = (T*)&lpFirst;
	}

	TbListT()
	{
		Null();
	}


};

template<class T>
class TbListNodeT
{
protected:
	T *lpNext, *lpPrev;
#ifdef _LB_DEBUG
	TbListT *lpList;
#endif

public:
};

#endif // ifdef __cplusplus
*/
//***************************************************************************
// LbList_Init : Initialise a linked list base
//***************************************************************************
void LbList_Init(TbList *lpList);

//***************************************************************************
// LbList_Insert : Insert the given node to the list at a position
//***************************************************************************
void LbList_Insert(TbList *lpList, TbListNode *lpAfterNode, TbListNode *lpNode);

//***************************************************************************
// LbList_Append
// LbList_Prepend
//***************************************************************************
#define LbList_Prepend(lpList, lpNode) LbList_Insert(lpList, LbList_BeforeFirst(lpList), lpNode)
#define LbList_Append(lpList, lpNode) LbList_Insert(lpList, (lpList)->lpLast, lpNode)

//***************************************************************************
// LbList_Remove : Remove the given node from the list
//		void LbList_Remove(TbListNode *lpNode);
//***************************************************************************
#ifndef _LB_DEBUG
#define LbList_Remove(lpNode) { (lpNode)->lpPrev->lpNext=(lpNode)->lpNext; (lpNode)->lpNext->lpPrev=(lpNode)->lpPrev; }
#else
void LbList_Remove(TbListNode *lpNode);
#endif

//***************************************************************************
// LbList_IsEmpty : Returns TRUE if the given list is empty
//		BOOL LbList_IsEmpty(const TbList *lpList);
//***************************************************************************
#ifndef _LB_DEBUG
#define LbList_IsEmpty(lpList) ((lpList)->lpFirst==(TbListNode*)&(lpList)->lpNull)
#else
BOOL LbList_IsEmpty(const TbList *lpList);
#endif

//***************************************************************************
// LbList_NodeExists : Verify a node is in the given list
//***************************************************************************
BOOL LbList_NodeExists(const TbList *lpList, const TbListNode *lpNode);

//***************************************************************************
// LbList_GetFirst & GetLast : Retrieve the first or last elements of a list. NULL for end.
//***************************************************************************
TbListNode * LbList_GetFirst(const TbList *lpList);
TbListNode * LbList_GetLast(const TbList *lpList);

//***************************************************************************
// LbList_GetNext & GetPrevious : Retrieve the next or prevoius in the list
//***************************************************************************
#ifndef _LB_DEBUG
#define LbList_GetNext(lpNode)		(lpNode)->lpNext
#define LbList_GetPrevious(lpNode)	(lpNode)->lpPrev
#else
TbListNode * LbList_GetNext(const TbListNode *lpNode);
TbListNode * LbList_GetPrevious(const TbListNode *lpNode);
#endif

//***************************************************************************
// LbList_IsLast & IsFirst : Establish if a node is first or last
//***************************************************************************
#ifndef _LB_DEBUG
#define LbList_IsLast(lpNode)		(!(lpNode)->lpNext)
#define LbList_IsFirst(lpNode)		(!(lpNode)->lpPrev)
#else
BOOL LbList_IsLast(const TbListNode *lpNode);
BOOL LbList_IsFirst(const TbListNode *lpNode);
#endif


#endif // ifndef _LBLISTS_H
//***************************************************************************
//				END OF FILE
//***************************************************************************
