#pragma once
#include "Pop3Error.h"
#include "EASTL/string.h"

#include "Poco/Format.h"

// =============================================================================
// Platform-independent exception signal constants.
// Values match the Windows SDK so existing serialized data is compatible.
// =============================================================================
#define POP3_EXCEPTION_ACCESS_VIOLATION          0xC0000005UL
#define POP3_EXCEPTION_ARRAY_BOUNDS_EXCEEDED     0xC000008CUL
#define POP3_EXCEPTION_BREAKPOINT                0x80000003UL
#define POP3_EXCEPTION_DATATYPE_MISALIGNMENT     0x80000002UL
#define POP3_EXCEPTION_FLT_DENORMAL_OPERAND      0xC000008DUL
#define POP3_EXCEPTION_FLT_DIVIDE_BY_ZERO        0xC000008EUL
#define POP3_EXCEPTION_FLT_INEXACT_RESULT        0xC000008FUL
#define POP3_EXCEPTION_FLT_INVALID_OPERATION     0xC0000090UL
#define POP3_EXCEPTION_FLT_OVERFLOW              0xC0000091UL
#define POP3_EXCEPTION_FLT_STACK_CHECK           0xC0000092UL
#define POP3_EXCEPTION_FLT_UNDERFLOW             0xC0000093UL
#define POP3_EXCEPTION_GUARD_PAGE                0x80000001UL
#define POP3_EXCEPTION_ILLEGAL_INSTRUCTION       0xC000001DUL
#define POP3_EXCEPTION_IN_PAGE_ERROR             0xC0000006UL
#define POP3_EXCEPTION_INT_DIVIDE_BY_ZERO        0xC0000094UL
#define POP3_EXCEPTION_INT_OVERFLOW              0xC0000095UL
#define POP3_EXCEPTION_INVALID_DISPOSITION       0xC0000026UL
#define POP3_EXCEPTION_INVALID_HANDLE            0xC0000008UL
#define POP3_EXCEPTION_NONCONTINUABLE_EXCEPTION  0xC0000025UL
#define POP3_EXCEPTION_PRIV_INSTRUCTION          0xC0000096UL
#define POP3_EXCEPTION_SINGLE_STEP               0x80000004UL
#define POP3_EXCEPTION_STACK_OVERFLOW            0xC00000FDUL
#define POP3_STATUS_UNWIND_CONSOLIDATE           0x80000029UL

// Opaque pointer to platform exception info.
// On Windows this is _EXCEPTION_POINTERS*; game code should not dereference it.
typedef void* Pop3ExceptionInfo;

// SEH translator function type — matches _se_translator_function signature.
typedef void (*Pop3SEHTranslator)(unsigned int, Pop3ExceptionInfo);

class Pop3MajorFault : public Pop3Exception
{
public:
    Pop3MajorFault() = default;
    Pop3MajorFault(const eastl::string wat) : _wat(wat) {}
    Pop3MajorFault(unsigned int signal, Pop3ExceptionInfo eptr) : _signal(signal), _eptr(eptr) { sort_out_signal_string(); }
    const eastl::string & wat() const { return _wat; }

    // Crash Data
    unsigned int        _signal = 0;
    Pop3ExceptionInfo   _eptr = nullptr;
private:
    void sort_out_signal_string()
    {
        switch (_signal)
        {
        case POP3_EXCEPTION_ACCESS_VIOLATION:
            _wat = "EXCEPTION_ACCESS_VIOLATION";
            break;
        case POP3_EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            _wat = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
            break;
        case POP3_EXCEPTION_BREAKPOINT:
            _wat = "EXCEPTION_BREAKPOINT";
            break;
        case POP3_EXCEPTION_DATATYPE_MISALIGNMENT:
            _wat = "EXCEPTION_DATATYPE_MISALIGNMENT";
            break;
        case POP3_EXCEPTION_FLT_DENORMAL_OPERAND:
            _wat = "EXCEPTION_FLT_DENORMAL_OPERAND";
            break;
        case POP3_EXCEPTION_FLT_DIVIDE_BY_ZERO:
            _wat = "EXCEPTION_FLT_DIVIDE_BY_ZERO";
            break;
        case POP3_EXCEPTION_FLT_INEXACT_RESULT:
            _wat = "EXCEPTION_FLT_INEXACT_RESULT";
            break;
        case POP3_EXCEPTION_FLT_INVALID_OPERATION:
            _wat = "EXCEPTION_FLT_INVALID_OPERATION";
            break;
        case POP3_EXCEPTION_FLT_OVERFLOW:
            _wat = "EXCEPTION_FLT_OVERFLOW";
            break;
        case POP3_EXCEPTION_FLT_STACK_CHECK:
            _wat = "EXCEPTION_FLT_STACK_CHECK";
            break;
        case POP3_EXCEPTION_FLT_UNDERFLOW:
            _wat = "EXCEPTION_FLT_UNDERFLOW";
            break;
        case POP3_EXCEPTION_GUARD_PAGE:
            _wat = "EXCEPTION_GUARD_PAGE";
            break;
        case POP3_EXCEPTION_ILLEGAL_INSTRUCTION:
            _wat = "EXCEPTION_ILLEGAL_INSTRUCTION";
            break;
        case POP3_EXCEPTION_IN_PAGE_ERROR:
            _wat = "EXCEPTION_IN_PAGE_ERROR";
            break;
        case POP3_EXCEPTION_INT_DIVIDE_BY_ZERO:
            _wat = "EXCEPTION_INT_DIVIDE_BY_ZERO";
            break;
        case POP3_EXCEPTION_INT_OVERFLOW:
            _wat = "EXCEPTION_INT_OVERFLOW";
            break;
        case POP3_EXCEPTION_INVALID_DISPOSITION:
            _wat = "EXCEPTION_INVALID_DISPOSITION";
            break;
        case POP3_EXCEPTION_INVALID_HANDLE:
            _wat = "EXCEPTION_INVALID_HANDLE";
            break;
        case POP3_EXCEPTION_NONCONTINUABLE_EXCEPTION:
            _wat = "EXCEPTION_NONCONTINUABLE_EXCEPTION";
            break;
        case POP3_EXCEPTION_PRIV_INSTRUCTION:
            _wat = "EXCEPTION_PRIV_INSTRUCTION";
            break;
        case POP3_EXCEPTION_SINGLE_STEP:
            _wat = "EXCEPTION_SINGLE_STEP";
            break;
        case POP3_EXCEPTION_STACK_OVERFLOW:
            _wat = "EXCEPTION_STACK_OVERFLOW";
            break;
        case POP3_STATUS_UNWIND_CONSOLIDATE:
            _wat = "STATUS_UNWIND_CONSOLIDATE";
            break;
        default:
            _wat = Poco::format("Unknown Operating System Exception, code %X", _signal).c_str();
        }
    }
    eastl::string _wat;
};
