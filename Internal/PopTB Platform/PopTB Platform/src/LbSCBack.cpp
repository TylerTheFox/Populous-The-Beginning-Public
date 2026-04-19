// LbSCBack.cpp - System event callback implementation
// Reimplemented from IDA disassembly of Bullfrog Library 8.1
#include "LbSCBack.h"
#include "Pop3Platform_Win32.h"

// Global system event callback instance
_LbSystemEventCallback _LbGCBS;

_LbSystemEventCallback::_LbSystemEventCallback()
{
    for (int i = 0; i < LBCB_NONE; i++)
        LbList_Init(&m_callbackList[i]);

    mpCriticalSection = new CRITICAL_SECTION;
    InitializeCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);
}

_LbSystemEventCallback::~_LbSystemEventCallback()
{
    DeleteAllCallbacks();
    DeleteCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);
    delete (CRITICAL_SECTION *)mpCriticalSection;
}

void _LbSystemEventCallback::EventNotification(TbSECallbackType type, void *param)
{
    EnterCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);

    if (type != LBCB_NONE)
    {
        TbListNode *node = LbList_GetFirst(&m_callbackList[type]);
        while (node && !LbList_IsLast(node))
        {
            ((_LbSEFunction *)node)->Notify(type, param);
            node = LbList_GetNext(node);
        }
    }

    LeaveCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);
}

void _LbSystemEventCallback::AddCallback(TbSECallbackType type, TbSECallback fn, void *userData)
{
    EnterCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);

    if (type != LBCB_NONE)
    {
        _LbSEFunction *node = new _LbSEFunction(fn, userData);
        LbList_Insert(&m_callbackList[type], m_callbackList[type].lpLast, (TbListNode *)node);
    }

    LeaveCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);
}

void _LbSystemEventCallback::DeleteCallback(TbSECallbackType type, TbSECallback fn)
{
    EnterCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);

    if (type != LBCB_NONE && !LbList_IsEmpty(&m_callbackList[type]))
    {
        TbListNode *node = LbList_GetFirst(&m_callbackList[type]);
        while (node && LbList_NodeExists(&m_callbackList[type], node))
        {
            if (((_LbSEFunction *)node)->m_fn == fn)
            {
                TbListNode *toDelete = node;
                node = LbList_GetNext(node);
                LbList_Remove(toDelete);
                delete toDelete;
            }
            else
            {
                node = LbList_GetNext(node);
            }
        }
    }

    LeaveCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);
}

void _LbSystemEventCallback::DeleteAllCallbacks()
{
    EnterCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);

    for (int i = 0; i < LBCB_NONE; i++)
    {
        while (!LbList_IsEmpty(&m_callbackList[i]))
        {
            TbListNode *node = LbList_GetFirst(&m_callbackList[i]);
            LbList_Remove(node);
            delete node;
        }
    }

    LeaveCriticalSection((LPCRITICAL_SECTION)mpCriticalSection);
}
