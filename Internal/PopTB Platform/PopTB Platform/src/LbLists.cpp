// LbLists.cpp - Linked list implementation
// Reimplemented from IDA disassembly of Bullfrog Library 8.1
#include <LbLists.h>

void LbList_Init(TbList *lpList)
{
    lpList->lpFirst = (TbListNode *)&lpList->lpNull;
    lpList->lpNull = NULL;
    lpList->lpLast = (TbListNode *)lpList;
}

void LbList_Insert(TbList *lpList, TbListNode *lpAfterNode, TbListNode *lpNode)
{
    lpNode->lpNext = lpAfterNode->lpNext;
    lpNode->lpPrev = lpAfterNode;
    lpAfterNode->lpNext->lpPrev = lpNode;
    lpAfterNode->lpNext = lpNode;
}

BOOL LbList_NodeExists(const TbList *lpList, const TbListNode *lpNode)
{
    for (TbListNode *n = lpList->lpFirst; n->lpNext; n = n->lpNext)
    {
        if (n == lpNode)
            return TRUE;
    }
    return FALSE;
}

TbListNode *LbList_GetFirst(const TbList *lpList)
{
    if (lpList->lpFirst == (TbListNode *)&lpList->lpNull)
        return NULL;
    return lpList->lpFirst;
}

TbListNode *LbList_GetLast(const TbList *lpList)
{
    if (lpList->lpLast == (TbListNode *)lpList)
        return NULL;
    return lpList->lpLast;
}
