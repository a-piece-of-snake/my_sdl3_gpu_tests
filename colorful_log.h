#pragma once
#include <SDL3/SDL.h>
#include <cstdarg>
#include <cstdio>
#include <print>

#define RESET     "\033[0m"
#define GRAY      "\033[1;30m"
#define RED       "\033[0;31m"
#define GREEN     "\033[0;32m"
#define YELLOW    "\033[0;33m"
#define BLUE      "\033[0;34m"
#define WHITE     "\033[0;37m"
#define BOLD_RED  "\033[1;31m"
#define BOLD_CYAN "\033[1;36m"

template <typename... Args>
inline void logMessage(const char* color, const char* level, const char* file, int line,
                       const char* fmt, bool flush, Args&&... args) {
    std::print("{}[{}]{} ", color, level, RESET);
    if (file != nullptr) {
        std::print("{}({}:{}){} ", GRAY, file, line, RESET);
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::printf(fmt, std::forward<Args>(args)...);
#pragma clang diagnostic pop // 被warn整没招了问了ai,解决了但是我也不知道这他妈是什么东西
    std::println("");

    if (flush) {
        std::fflush(stdout);
    }
}

inline void SDLCALL MySDLLogOutput(void* userdata, int category, SDL_LogPriority priority,
                                   const char* message) {
    const char* color = WHITE;
    const char* level = "INFO";

    switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
        color = GRAY;
        level = "VERB";
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        color = BLUE;
        level = "DEBUG";
        break;
    case SDL_LOG_PRIORITY_INFO:
        color = WHITE;
        level = "INFO";
        break;
    case SDL_LOG_PRIORITY_WARN:
        color = YELLOW;
        level = "WARN";
        break;
    case SDL_LOG_PRIORITY_ERROR:
        color = RED;
        level = "ERROR";
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        color = BOLD_RED;
        level = "CRIT";
        break;
    default:
        break;
    }

    std::println("{}[{}-SDL]{} {}", color, level, RESET, message);
}

#define LOG_INFO(fmt, ...)  logMessage(WHITE, "INFO", __FILE__, __LINE__, fmt, false, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) logMessage(GRAY, "DEBUG", __FILE__, __LINE__, fmt, false, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  logMessage(YELLOW, "WARN", __FILE__, __LINE__, fmt, true, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) logMessage(RED, "ERROR", __FILE__, __LINE__, fmt, true, ##__VA_ARGS__)
#define LOG_SUCCESS(fmt, ...)                                                                      \
    logMessage(GREEN, "SUCCESS", __FILE__, __LINE__, fmt, false, ##__VA_ARGS__)
