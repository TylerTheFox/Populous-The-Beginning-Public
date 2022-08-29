#pragma once
#include <fstream>
#include "../../../Build.h"
#include "Poco/Mutex.h"
#include <vector>

#if CM_DEVELOPMENT
#define Pop3DebugAssert 1
#else
#define Pop3DebugAssert 0
#endif

#define MAX_DEBUG_OUPUT 50

#define THROW_SEH_EXCEPTION() ( *static_cast<int*>(0) = 1 )
#define POP3DEBUG_BUFFER_SIZE 10000
class Pop3Debug
{
public:
    Pop3Debug() = delete;

    static bool openLog(const std::string & file);
    static void closeLog();
    static void renameLogFile(const std::string & newName);

    static void trace(const char* fmt, ...);
    static void trace(const wchar_t* fmt, ...);
    static void trace_no_new_line(const char* fmt, ...);

    static void assertmsg(bool b, const char * msg);

    static void fatalError(const char* fmt, ...);
    static void fatalError_NoReport(const char* fmt, ...);
    static std::string getLogPath();

    static std::vector<std::string> & getOutputVect();
private:
    static void writeToLog(const std::string & data);
    static void writeToLog(const char* data);

    static void writeToOutput(const char* data);
    static void writeToOutput(const wchar_t* data);

    static char                 _buffer[POP3DEBUG_BUFFER_SIZE];
    static std::string          _log;
    static std::ofstream        _out;
    static Poco::Mutex          _mu;
    static std::vector<std::string> _debugLog;
};

#define TRACE(s)                Pop3Debug::trace(s)
#define TRACEW(s)               Pop3Debug::tracew(s)

#if Pop3DebugAssert
#define ASSERTMSG(condition, m) { Pop3Debug::assertmsg(condition, m); }
#else
#define ASSERTMSG(condition, m)
#endif
#define ASSERT(condition)       ASSERTMSG(condition, "");

#if Pop3DebugAssert
#define VERIFY ASSERT
#else
#define VERIFY(s) (s)
#endif