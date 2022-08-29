#pragma once
#include "EASTL/string.h"

class Pop3Exception
{
public:
    Pop3Exception() = default;
    Pop3Exception(const eastl::string & wat) : _wat(wat) {}
    virtual const eastl::string & wat() const { return _wat; }
private:
    eastl::string _wat;
};