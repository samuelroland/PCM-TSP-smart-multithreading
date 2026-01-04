// Basic log system to easily enable or disable logging at a given level
// Source: ChatGPT
#include <iostream>
#include <sstream>
#pragma once

enum class LogLevel {
    NONE = 0,
    DEBUG = 1,
    TRACE = 2
};

constexpr LogLevel GLOBAL_LOG_LEVEL = LogLevel::NONE;

struct LogLine {
    std::ostringstream ss;
    std::ostream& out;

    explicit LogLine(std::ostream& o) : out(o) {}

    ~LogLine() {
        ss << std::endl;
        out << ss.str();
    }

    template<typename T>
    LogLine& operator<<(T&& v) {
        ss << std::forward<T>(v);
        return *this;
    }
};

#define LOG_LEVEL_ENABLED(level) \
    (static_cast<int>(level) <= static_cast<int>(GLOBAL_LOG_LEVEL))

#define TRACE                                            \
    if constexpr (!LOG_LEVEL_ENABLED(LogLevel::TRACE)) { \
    } else                                               \
        LogLine{std::cout} <<

#define DEBUG                                            \
    if constexpr (!LOG_LEVEL_ENABLED(LogLevel::DEBUG)) { \
    } else                                               \
        LogLine{std::cout} <<

#define NONE    \
    if (true) { \
    } else      \
        LogLine{std::cout} <<
