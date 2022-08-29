#pragma once
#include "EASTL/string.h"
#include "Pop3Error.h"

#include <string>
#include <Poco/Format.h>

class Pop3AssertionException : public Pop3Exception
{
public:
    Pop3AssertionException() = default;
    Pop3AssertionException(const eastl::string & wat, const eastl::string & file, unsigned long long line) : _wat_reason(wat), _file(file), _line(line) 
    { 
        auto file_ = std::string(file.c_str());
        auto msg = std::string(wat.c_str());
        _wat = eastl::string(Poco::format("ASSERTION ERROR: %s:%Lu -- %s", file_, _line, msg).c_str());
    }
    Pop3AssertionException(const eastl::string & wat) : _wat(wat) {}
    const eastl::string & wat() const { return _wat; }
private:
    eastl::string       _file;
    unsigned long long  _line;
    eastl::string       _wat_reason;
    eastl::string       _wat;
};