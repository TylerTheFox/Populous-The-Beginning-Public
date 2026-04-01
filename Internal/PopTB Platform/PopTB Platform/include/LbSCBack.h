#pragma once
//***************************************************************************
// LbSystemCallback
//***************************************************************************


//***************************************************************************
#include <LbLists.h>
#include <Pop3Size.h>
//***************************************************************************
#ifdef __cplusplus

extern "C"
{
#endif
    //***************************************************************************
    //		System Event classes  SE's
    //***************************************************************************

    typedef enum TbSECallbackType
    {
        LBCB_SCREEN_BEGIN_SWAP = 0,
        LBCB_SCREEN_END_SWAP,
        LBCB_SCREEN_BEGIN_FLIP,
        LBCB_SCREEN_END_FLIP,
        LBCB_POINTER_CB_EVENT,
        LBCB_MOUSE_MOVE,
        LBCB_SCREEN_ACTIVATE,
        LBCB_SCREEN_MODECHANGE,
        LBCB_NONE,
    } TbSECallbackType;

    typedef void (__stdcall *TbSECallback)(TbSECallbackType, void*, void*);

    class _LbSEFunction : public TbListNode
    {
    public:
        _LbSEFunction(TbSECallback fn = NULL, void* userData = NULL)
        {
            m_fn = fn;
            mUserData = userData;
        }

        void Notify(TbSECallbackType type, void* param)
        {
            if (m_fn)(m_fn)(type, param, mUserData);
        }

        void* mUserData;
        TbSECallback m_fn;
    };

    class _LbSystemEventCallback
    {
    public:
        _LbSystemEventCallback();
        ~_LbSystemEventCallback();

        void AddCallback(TbSECallbackType type, TbSECallback fn, void* userData = NULL);
        void DeleteCallback(TbSECallbackType type, TbSECallback fn);

        void EventNotification(TbSECallbackType type, void* param);

    protected:
        void DeleteAllCallbacks();

        TbList m_callbackList[LBCB_NONE];
        void* mpCriticalSection;
    };


    // Global system event callback
    extern _LbSystemEventCallback _LbGCBS;

    //***************************************************************************
#ifdef __cplusplus
}
#endif

class _CBINFO
{
    LB_PUBLIC_MEMBERS
    enum _eventtype
    {
        POINTER_RELEASE,
        POINTER_HIDE,
        POINTER_BACKUP,
        POINTER_RESTORE
    } mEventType; // Type of event

    union
    {
        ULONG mParam; // Parameter, depending on event type
        struct
        {
            UINT mWidth; // A size, used by POINTER_RESTORE event
            UINT mHeight;
        };
    };

    _CBINFO(_eventtype eventtype, ULONG param = 0)
    {
        mEventType = eventtype;
        mParam = param;
    };

    _CBINFO(_eventtype eventtype, Pop3Size& size)
    {
        mEventType = eventtype;
        mWidth = size.Width;
        mHeight = size.Height;
    };
};


typedef TbSECallbackType TbSysMessage;
typedef TbSECallback TbSysCallback;

inline void LbApp_AddSystemCallback(TbSysMessage message, TbSysCallback fn)
{
    // Add to global system callbacks.
    _LbGCBS.AddCallback(message, fn);
}

inline void LbApp_DeleteSystemCallback(TbSysMessage message, TbSysCallback fn)
{
    _LbGCBS.DeleteCallback(message, fn);
}

//***************************************************************************

