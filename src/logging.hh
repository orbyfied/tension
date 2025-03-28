#pragma once

#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <filesystem>
#include <iostream>
#include "util/ansi.hh"
#include <sys/time.h>

#ifndef TC_DEBUG_LOG
#define TC_DEBUG_LOG 1
#endif

typedef unsigned long long Time;

inline float to_seconds(Time time) {
    return time / 1'000'000.0;
}

inline Time get_microseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1'000'000ULL + tv.tv_usec;
}

static Time startMicrosec;

inline Time get_microseconds_since_start() {
    if (startMicrosec <= 0) {
        startMicrosec = get_microseconds();
    }

    return get_microseconds() - startMicrosec;   
}

/// @brief The code position.
struct CodePos {
    char* filename;
    int line;
};

enum Level {
    INFO,
    WARN,
    ERR,
    DEBUG
};

#define P CodePos{ .filename = __FILE__, .line = __LINE__ }

template<Level level>
static Time log(CodePos pos, const char* fmt...) {
    std::filesystem::path p = std::filesystem::path(pos.filename);
    p = std::filesystem::relative(p, std::filesystem::path("src/"));

    // print time
    Time us = get_microseconds_since_start();
    float seconds = us / 1'000'000.0;

    printf(MAG "%f ", seconds);

    switch (level)
    {
    case INFO:
        printf(CYN "INFO  "); break;
    case WARN:
        printf(YEL "WARN  "); break;
    case ERR:
        printf(RED "ERROR "); break;
    case DEBUG:
        printf(BLU "DEBUG "); break;
    default:
        return us;
    }

    va_list args;
    va_start(args, fmt);
    std::cout << UWHT "" << p.string() << ":" << pos.line << "" CRESET " ";
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    return us;
}

#if TC_DEBUG_LOG != 1
template<>
inline Time log<DEBUG>(CodePos pos, const char* fmt...) {
    return get_microseconds_since_start();
}
#endif