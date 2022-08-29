#pragma once
#include "Pop3Error.h"
#include "EASTL/string.h"
class Pop3ScriptException : public Pop3Exception
{
public:
    Pop3ScriptException() = default;
    Pop3ScriptException(const eastl::string wat) : _wat(wat) {}
    const eastl::string & wat() const { return _wat; }
private:
    eastl::string _wat;
};