#include "messages.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define LEVEL_INFO 1
#define LEVEL_WARN 2
#define LEVEL_ERROR 3
#define LEVEL_FATAL 4

static void show_error(const char *msg, bool fatal)
{
#ifdef WIN32
    MessageBox(NULL, msg, fatal ? "Fatal error" : "Error", MB_OK|MB_ICONERROR);
#else
    printf("%s: %s\n", fatal ? "FATAL" : "ERROR", msg);
#endif
}

static void message(int level, const char *fmt, va_list ap)
{
    char msg[1024] = "";

    vsnprintf(msg, sizeof(msg), fmt, ap);
    
    switch (level)
    {
    default:
    case LEVEL_INFO:
        fprintf(stderr, "%s\n", msg);
        break;

    case LEVEL_WARN:
        fprintf(stderr, "WARNING: %s\n", msg);
        break;
    
    case LEVEL_ERROR:
        show_error(msg, false);
        break;

    case LEVEL_FATAL:
        show_error(msg, true);
        break;
    }
}

void info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(LEVEL_INFO, fmt, ap);
    va_end(ap);
}

void warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(LEVEL_WARN, fmt, ap);
    va_end(ap);
}

void error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(LEVEL_FATAL, fmt, ap);
    va_end(ap);
    abort();
}
