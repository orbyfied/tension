#pragma once

#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <filesystem>
#include <iostream>
#include "ansi.hh"

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

// Logging function including code position
static void log(CodePos pos, Level level, const char* fmt...) {
    std::filesystem::path p = std::filesystem::path(pos.filename);
    p = std::filesystem::relative(p, std::filesystem::path("src/"));

    switch (level)
    {
    case INFO:
        printf(CYN "INFO "); break;
    case WARN:
        printf(YEL "WARN "); break;
    case ERR:
        printf(RED "ERR  "); break;
    case DEBUG:
        printf(BLU "DBG  "); break;
    default:
        return;
    }

    va_list args;
    va_start(args, fmt);
    std::cout << UWHT " " << p.string() << ":" << pos.line << " " CRESET " ";
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}