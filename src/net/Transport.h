// net/Transport.h
// Simulated transport with latency, drops, and partitions.
#pragma once
#include <functional>
#include <string>
#include <chrono>
#include <random>
#include <memory>
#include "util/Error.h"

struct NetworkParams
{
    std::chrono::milliseconds latency{50};
    double dropRate{0.01};
};

class TransportImpl;
class DetailedLogger;

class Transport
{
public:
    using Bytes = std::string;
    using DeliverFn = std::function<void(const Bytes &)>;
    struct Endpoint
    {
        DeliverFn deliver;
    };
    Transport(unsigned seed, NetworkParams params, DetailedLogger* detailedLogger = nullptr);
    ~Transport();

    // Register a mailbox identified by peer address; returns Status.
    Status registerEndpoint(const std::string &address, DeliverFn deliver);

    // Asynchronous send with simulated latency/drops.
    Status send(const std::string &from, const std::string &to, const Bytes &data);

    void setParams(NetworkParams p);

    // Unregister a mailbox
    Status unregisterEndpoint(const std::string &address);

    // Wait for all pending deliveries to complete before shutdown
    void waitForPendingDeliveries();

    // Shutdown the transport thread pool
    void shutdown();

private:
    std::unique_ptr<class TransportImpl> impl_;
};
