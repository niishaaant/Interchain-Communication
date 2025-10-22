// filepath: /home/niishaaant/work/blockchain-comm-sim/src/net/Transport.cpp
#include "Transport.h"
#include "util/DetailedLogger.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <random>

using namespace std::chrono_literals;

namespace
{
    // Helper for random drop
    bool shouldDrop(std::mt19937 &rng, double dropRate)
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < dropRate;
    }
}

class TransportImpl
{
public:
    TransportImpl(unsigned seed, NetworkParams params, DetailedLogger* detailedLogger)
        : params_(params), rng_(seed), detailedLogger_(detailedLogger) {}

    ~TransportImpl()
    {
        for (auto &t : threads_)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
    }

    Status registerEndpoint(const std::string &address, Transport::DeliverFn deliver)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (endpoints_.count(address))
        {
            return {ErrorCode::InvalidState, "Endpoint already registered"};
        }
        endpoints_[address] = {deliver};
        return {ErrorCode::Ok, ""};
    }

    Status send(const std::string &from, const std::string &to, const Transport::Bytes &data)
    {

        for (auto &t : threads_)
        {
            if (t.joinable())
            {
                // They should have finished after params_.latency; join now.
                t.join();
            }
        }
        threads_.clear();

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!endpoints_.count(to))
            {
                return {ErrorCode::NotFound, "Destination endpoint not found"};
            }
        }
        // Simulate drop
        if (shouldDrop(rng_, params_.dropRate))
        {
            // Log network drop
            if (detailedLogger_)
            {
                detailedLogger_->logNetworkDrop(
                    from,
                    to,
                    "unknown",  // message type - could be inferred from data
                    data.size(),
                    "random_drop"
                );
            }
            return {ErrorCode::NetworkDrop, "Packet dropped by network"};
        }
        // Simulate latency and deliver asynchronously
        threads_.emplace_back([this, to, data]()
                              {
            std::this_thread::sleep_for(params_.latency);
            Transport::DeliverFn deliver;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = endpoints_.find(to);
                if (it == endpoints_.end()) return;
                deliver = it->second.deliver;
            }
            if (deliver) deliver(data); });
        return {ErrorCode::Ok, ""};
    }

    void setParams(NetworkParams p)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        params_ = p;
    }

private:
    std::unordered_map<std::string, Transport::Endpoint> endpoints_;
    NetworkParams params_;
    std::mt19937 rng_;
    std::mutex mtx_;
    std::vector<std::thread> threads_; // Added to manage async sends
    DetailedLogger* detailedLogger_;
};

// Implementation forwarding to TransportImpl

Transport::Transport(unsigned seed, NetworkParams params, DetailedLogger* detailedLogger)
    : impl_(std::make_unique<TransportImpl>(seed, params, detailedLogger)) {}

Transport::~Transport() = default;

Status Transport::registerEndpoint(const std::string &address, DeliverFn deliver)
{
    return impl_->registerEndpoint(address, deliver);
}

Status Transport::send(const std::string &from, const std::string &to, const Bytes &data)
{
    return impl_->send(from, to, data);
}

void Transport::setParams(NetworkParams p)
{
    impl_->setParams(p);
}