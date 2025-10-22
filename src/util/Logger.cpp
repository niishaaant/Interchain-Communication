// filepath: /home/niishaaant/work/blockchain-comm-sim/src/util/Logger.cpp
#include "Logger.h"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <thread>

namespace
{
    std::mutex log_mutex;

    const char *levelToString(LogLevel lvl)
    {
        switch (lvl)
        {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    // Returns current time as string: YYYY-MM-DD HH:MM:SS
    std::string currentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto t_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#if defined(_WIN32)
        localtime_s(&tm, &t_c);
#else
        localtime_r(&t_c, &tm);
#endif
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
        return buf;
    }
}

Logger::Logger(std::string name)
    : name_(std::move(name))
{
}

void Logger::setLevel(LogLevel lvl)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    level_ = lvl;
}

void Logger::trace(const std::string &msg)
{
    if (level_ <= LogLevel::Trace)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[" << currentTime() << "]"
                  << " [" << levelToString(LogLevel::Trace) << "]"
                  << " [" << name_ << "]"
                  << " [TID:" << std::this_thread::get_id() << "] "
                  << "[Type: TRACE] "
                  << msg << std::endl;
    }
}

void Logger::debug(const std::string &msg)
{
    if (level_ <= LogLevel::Debug)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[" << currentTime() << "]"
                  << " [" << levelToString(LogLevel::Debug) << "]"
                  << " [" << name_ << "]"
                  << " [TID:" << std::this_thread::get_id() << "] "
                  << "[Type: DEBUG] "
                  << msg << std::endl;
    }
}

void Logger::info(const std::string &msg)
{
    if (level_ <= LogLevel::Info)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[" << currentTime() << "]"
                  << " [" << levelToString(LogLevel::Info) << "]"
                  << " [" << name_ << "]"
                  << " [TID:" << std::this_thread::get_id() << "] "
                  << "[Type: INFO] "
                  << msg << std::endl;
    }
}

void Logger::warn(const std::string &msg)
{
    if (level_ <= LogLevel::Warn)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cout << "[" << currentTime() << "]"
                  << " [" << levelToString(LogLevel::Warn) << "]"
                  << " [" << name_ << "]"
                  << " [TID:" << std::this_thread::get_id() << "] "
                  << "[Type: WARN] "
                  << msg << std::endl;
    }
}

void Logger::error(const std::string &msg)
{
    if (level_ <= LogLevel::Error)
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << "[" << currentTime() << "]"
                  << " [" << levelToString(LogLevel::Error) << "]"
                  << " [" << name_ << "]"
                  << " [TID:" << std::this_thread::get_id() << "] "
                  << "[Type: ERROR] "
                  << msg << std::endl;
    }
}