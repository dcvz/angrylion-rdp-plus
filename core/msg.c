#include "msg.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define MSG_BUFFER_LEN 256

void msg_error(const char * err, ...)
{
    char buf[MSG_BUFFER_LEN];
    va_list arg;
    va_start(arg, err);
#ifdef WIN32
    vsprintf_s(buf, sizeof(buf), err, arg);
    MessageBoxA(0, buf, "RDP: fatal error", MB_OK);
#else
    vprintf(buf, arg);
#endif
    va_end(arg);
    exit(0);
}

void msg_warning(const char* err, ...)
{
    char buf[MSG_BUFFER_LEN];
    va_list arg;
    va_start(arg, err);
#ifdef WIN32
    vsprintf_s(buf, sizeof(buf), err, arg);
    MessageBoxA(0, buf, "RDP: warning", MB_OK);
#else
    vprintf(buf, arg);
#endif
    va_end(arg);
}

void msg_debug(const char* err, ...)
{
    char buf[MSG_BUFFER_LEN];
    va_list arg;
    va_start(arg, err);
#ifdef WIN32
    vsprintf_s(buf, sizeof(buf), err, arg);
    OutputDebugStringA(buf);
#else
    vprintf(buf, arg);
#endif
    va_end(arg);
}
