// util/Metrics.h
// Sink for counters, gauges, histograms (namespaced by chain/node).
#pragma once
#include <string>
#include <chrono>
#include <fstream>
#include <mutex>

class MetricsSink
{
public:
    MetricsSink(const std::string& filename = "metrics.jsonl"); // Constructor
    ~MetricsSink(); // Destructor

    void incCounter(const std::string &name, double delta = 1.0);
    void setGauge(const std::string &name, double value);
    void observe(const std::string &name, double value);
    
    // You can add a more generic method for complex events
    void logEvent(const std::string& json_payload);

private:
    std::ofstream outFile_;
    std::mutex mutex_; // To make it thread-safe
};
