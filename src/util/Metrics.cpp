// filepath: /home/niishaaant/work/blockchain-comm-sim/src/util/Metrics.cpp
#include "Metrics.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <stdexcept>

// Helper: escape JSON string (basic)
static std::string escapeJsonString(const std::string &input)
{
    std::ostringstream ss;
    for (char c : input)
    {
        switch (c)
        {
        case '\"':
            ss << "\\\"";
            break;
        case '\\':
            ss << "\\\\";
            break;
        case '\b':
            ss << "\\b";
            break;
        case '\f':
            ss << "\\f";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                ss << "\\u"
                   << std::hex << std::setw(4) << std::setfill('0')
                   << static_cast<int>(static_cast<unsigned char>(c))
                   << std::dec << std::setfill(' ');
            }
            else
            {
                ss << c;
            }
        }
    }
    return ss.str();
}

// Helper: ISO8601 timestamp with milliseconds (UTC)
static std::string nowIso8601()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setw(3) << std::setfill('0') << ms.count() << 'Z';
    return ss.str();
}

MetricsSink::MetricsSink(const std::string &filename)
{
    outFile_.open(filename, std::ofstream::out | std::ofstream::app);
    if (!outFile_.is_open())
    {
        throw std::runtime_error("MetricsSink: failed to open file: " + filename);
    }
}

MetricsSink::~MetricsSink()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (outFile_.is_open())
    {
        outFile_.flush();
        outFile_.close();
    }
}

void MetricsSink::logEvent(const std::string &json_payload)
{
    // json_payload is expected to be valid JSON (object or value).
    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJsonString(nowIso8601()) << "\",";
    // thread id as numeric via hash
    ss << "\"thread\":" << std::hash<std::thread::id>{}(std::this_thread::get_id()) << ",";
    ss << "\"payload\":";
    ss << json_payload;
    ss << "}";

    std::lock_guard<std::mutex> lock(mutex_);
    outFile_ << ss.str() << '\n';
    outFile_.flush();
}

void MetricsSink::incCounter(const std::string &name, double delta)
{
    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJsonString(nowIso8601()) << "\",";
    ss << "\"type\":\"counter\",";
    ss << "\"name\":\"" << escapeJsonString(name) << "\",";
    ss << "\"delta\":" << std::showpoint << delta << ",";
    ss << "\"thread\":" << std::hash<std::thread::id>{}(std::this_thread::get_id());
    ss << "}";

    std::lock_guard<std::mutex> lock(mutex_);
    outFile_ << ss.str() << '\n';
    outFile_.flush();
}

void MetricsSink::setGauge(const std::string &name, double value)
{
    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJsonString(nowIso8601()) << "\",";
    ss << "\"type\":\"gauge\",";
    ss << "\"name\":\"" << escapeJsonString(name) << "\",";
    ss << "\"value\":" << std::showpoint << value << ",";
    ss << "\"thread\":" << std::hash<std::thread::id>{}(std::this_thread::get_id());
    ss << "}";

    std::lock_guard<std::mutex> lock(mutex_);
    outFile_ << ss.str() << '\n';
    outFile_.flush();
}

void MetricsSink::observe(const std::string &name, double value)
{
    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJsonString(nowIso8601()) << "\",";
    ss << "\"type\":\"histogram\",";
    ss << "\"name\":\"" << escapeJsonString(name) << "\",";
    ss << "\"value\":" << std::showpoint << value << ",";
    ss << "\"thread\":" << std::hash<std::thread::id>{}(std::this_thread::get_id());
    ss << "}";

    std::lock_guard<std::mutex> lock(mutex_);
    outFile_ << ss.str() << '\n';
    outFile_.flush();
}