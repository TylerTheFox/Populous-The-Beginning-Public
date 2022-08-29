#include "Pop3Debug.h"
#include <iostream>
#include <string>
#include <assert.h>
#include <Windows.h>
#include <SDL.h>
#include "Pop3App.h"
#include "Poco/File.h"
#include <debugapi.h>
#include <Poco/Format.h>

std::string                 Pop3Debug::_log;
std::ofstream               Pop3Debug::_out;
char                        Pop3Debug::_buffer[POP3DEBUG_BUFFER_SIZE];
Poco::Mutex                 Pop3Debug::_mu;
std::vector<std::string>    Pop3Debug::_debugLog;

bool Pop3Debug::openLog(const std::string & file)
{
    closeLog();
    _log = file;
    _out.open(file, std::ios::trunc);
    return _out.is_open();
}

void Pop3Debug::closeLog()
{
    if (_out.is_open())
        _out.close();
}

void Pop3Debug::renameLogFile(const std::string & newName)
{
    if (_out.is_open()) closeLog();
    Poco::File oldLogFile(_log);
    if (oldLogFile.exists())
        oldLogFile.renameTo(newName);
    openLog(_log);
}

void Pop3Debug::trace(const char *fmt, ...)
{
    _mu.lock();
    va_list args;
    va_start(args, fmt);
    vsnprintf(_buffer, POP3DEBUG_BUFFER_SIZE, fmt, args);

    static int i;
    for (i = 0; i < 1024; i++)
        if (!_buffer[i]) break;

    _buffer[i++] = '\n';
    _buffer[i]   = 0;

    writeToOutput(_buffer);
    writeToLog(_buffer);
    va_end(args);
    _mu.unlock();
}

void Pop3Debug::trace(const wchar_t* fmt, ...)
{
    _mu.lock();
    static const wchar_t* pt;
    static size_t i, length;
    static char * buffer;
    wchar_t* buff = reinterpret_cast<wchar_t*>(&_buffer[0]);
    va_list args;
    va_start(args, fmt);
    vswprintf(buff, POP3DEBUG_BUFFER_SIZE, fmt, args);
    if (!buffer) buffer = new char[MB_CUR_MAX];

    pt = buff;
    while (*pt) 
    {
        length = wctomb(buffer, *pt);
        if (length < 1) break;
        for (i = 0; i < length; ++i) printf("[%c]", buffer[i]);
        ++pt;
    }
    i++;
    buffer[i] = 0;

    writeToOutput(buffer);
    writeToLog(buffer);   
    _mu.unlock();
}

void Pop3Debug::trace_no_new_line(const char *fmt, ...)
{
    _mu.lock();
    va_list args;
    va_start(args, fmt);
    vsnprintf(_buffer, POP3DEBUG_BUFFER_SIZE, fmt, args);

    static int i;
    for (i = 0; i < 1024; i++)
        if (!_buffer[i]) break;

    _buffer[i] = 0;

    writeToOutput(_buffer);
    writeToLog(_buffer);
    va_end(args);
    _mu.unlock();
}

extern ULONGLONG getGameClockMiliseconds();
void Pop3Debug::writeToLog(const std::string& data)
{
    _out << "[" << getGameClockMiliseconds() << "]: " << data;
    _out.flush();
}

void Pop3Debug::writeToLog(const char* data)
{
    _out << "[" << getGameClockMiliseconds() << "]: " << data;
    _out.flush();
}

void Pop3Debug::writeToOutput(const char* data)
{
#ifdef _WIN32
    if (_debugLog.size() > MAX_DEBUG_OUPUT)
        _debugLog.erase(_debugLog.begin());

    _debugLog.push_back(data);
    OutputDebugString(data);
#endif
}

void Pop3Debug::writeToOutput(const wchar_t * data)
{
    static const wchar_t* pt;
    static size_t i, length;
    static char * buffer;
    if (!buffer) buffer = new char[MB_CUR_MAX];

    pt = data;
    while (*pt) {
        length = wctomb(buffer, *pt);
        if (length < 1) break;
        for (i = 0; i < length; ++i) printf("[%c]", buffer[i]);
        ++pt;
    }
    i++;
    buffer[i] = 0;
    writeToOutput(buffer);
}


void Pop3Debug::assertmsg(bool b, const char * msg)
{
    if (!b)
    {
        fatalError(msg);
    }
}

std::string Pop3Debug::getLogPath()
{
    return _log;
}

std::vector<std::string>& Pop3Debug::getOutputVect()
{
    return _debugLog;
}

void Pop3Debug::fatalError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(_buffer, POP3DEBUG_BUFFER_SIZE, fmt, args);
    trace(_buffer);
    auto s = Poco::format("Fatal Error: %s", std::string(_buffer));

#if POP3_BUILD_USE_SDL2
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PopTB Error", "Game has encountered a fatal error, check logs.", Pop3App::getWindow());
#else
    MessageBox(nullptr, TEXT(s.c_str()), TEXT("PopTB Error"), MB_OK);
#endif

    THROW_SEH_EXCEPTION();
}

void Pop3Debug::fatalError_NoReport(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(_buffer, POP3DEBUG_BUFFER_SIZE, fmt, args);
    trace(_buffer);

#if POP3_BUILD_USE_SDL2
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PopTB Error", "Game has encountered a fatal error, check logs.", Pop3App::getWindow());
#else
    MessageBox(nullptr, TEXT(_buffer), TEXT("PopTB Error"), MB_OK);
#endif

    exit(-1);
}

