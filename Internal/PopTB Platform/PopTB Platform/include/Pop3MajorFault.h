#pragma once
#include "Pop3Error.h"
#include "EASTL/string.h"
#include "Windows.h"

#include "Poco/Format.h"

class Pop3MajorFault : public Pop3Exception
{
public:
    Pop3MajorFault() = default;
    Pop3MajorFault(const eastl::string wat) : _wat(wat) {}
    Pop3MajorFault(unsigned int signal, struct _EXCEPTION_POINTERS* eptr) : _signal(signal), _eptr(eptr) { sort_out_signal_string(); }
    const eastl::string & wat() const { return _wat; }

    // Crash Data
    int _signal;
    _EXCEPTION_POINTERS* _eptr;
private:
    void sort_out_signal_string()
    {
        switch (_signal)
        {
        case EXCEPTION_ACCESS_VIOLATION:
            _wat = "EXCEPTION_ACCESS_VIOLATION";
            break;
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            _wat = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
            break;
        case EXCEPTION_BREAKPOINT:
            _wat = "EXCEPTION_BREAKPOINT";
            break;
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            _wat = "EXCEPTION_DATATYPE_MISALIGNMENT";
            break;
        case EXCEPTION_FLT_DENORMAL_OPERAND:
            _wat = "EXCEPTION_FLT_DENORMAL_OPERAND";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            _wat = "EXCEPTION_FLT_DIVIDE_BY_ZERO";
            break;
        case EXCEPTION_FLT_INEXACT_RESULT:
            _wat = "EXCEPTION_FLT_INEXACT_RESULT";
            break;
        case EXCEPTION_FLT_INVALID_OPERATION:
            _wat = "EXCEPTION_FLT_INVALID_OPERATION";
            break;
        case EXCEPTION_FLT_OVERFLOW:
            _wat = "EXCEPTION_FLT_OVERFLOW";
            break;
        case EXCEPTION_FLT_STACK_CHECK:
            _wat = "EXCEPTION_FLT_STACK_CHECK";
            break;
        case EXCEPTION_FLT_UNDERFLOW:
            _wat = "EXCEPTION_FLT_UNDERFLOW";
            break;
        case EXCEPTION_GUARD_PAGE:
            _wat = "EXCEPTION_GUARD_PAGE";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            _wat = "EXCEPTION_ILLEGAL_INSTRUCTION";
            break;
        case EXCEPTION_IN_PAGE_ERROR:
            _wat = "EXCEPTION_IN_PAGE_ERROR";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            _wat = "EXCEPTION_INT_DIVIDE_BY_ZERO";
            break;
        case EXCEPTION_INT_OVERFLOW:
            _wat = "EXCEPTION_INT_OVERFLOW";
            break;
        case EXCEPTION_INVALID_DISPOSITION:
            _wat = "EXCEPTION_INVALID_DISPOSITION";
            break;
        case EXCEPTION_INVALID_HANDLE:
            _wat = "EXCEPTION_INVALID_HANDLE";
            break;
        case EXCEPTION_NONCONTINUABLE_EXCEPTION:
            _wat = "EXCEPTION_NONCONTINUABLE_EXCEPTION";
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            _wat = "EXCEPTION_PRIV_INSTRUCTION";
            break;
        case EXCEPTION_SINGLE_STEP:
            _wat = "EXCEPTION_SINGLE_STEP";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            _wat = "EXCEPTION_STACK_OVERFLOW";
            break;
        case STATUS_UNWIND_CONSOLIDATE:
            _wat = "STATUS_UNWIND_CONSOLIDATE";
            break;
        default:
            _wat = Poco::format("Unknown Operating System Exception, code %X", _signal).c_str();
        }
    }
    eastl::string _wat;
};