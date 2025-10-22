// util/Logger.h
// Minimal structured logger with levels, thread-safe.
#pragma once
#include <string>
#include <atomic>

enum class LogLevel
{
    Trace,
    Debug,
    Info,
    Warn,
    Error
};

class Logger
{
public:
    explicit Logger(std::string name);
    void setLevel(LogLevel lvl);
    void trace(const std::string &msg);
    void debug(const std::string &msg);
    void info(const std::string &msg);
    void warn(const std::string &msg);
    void error(const std::string &msg);

private:
    std::string name_;
    LogLevel level_{LogLevel::Info};
};
