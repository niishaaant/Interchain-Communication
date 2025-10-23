// filepath: /home/niishaaant/work/blockchain-comm-sim/src/net/Transport.cpp
#include "Transport.h"
#include "util/DetailedLogger.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <random>
#include <memory>
#include <iostream>

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

// Shared state for TransportImpl to allow detached threads to safely access it
struct TransportState
{
    std::unordered_map<std::string, Transport::Endpoint> endpoints_;
    std::mutex mtx_;
};

class TransportImpl
{
public:
    TransportImpl(unsigned seed, NetworkParams params, DetailedLogger *detailedLogger)
        : params_(params), rng_(seed), detailedLogger_(detailedLogger), state_(std::make_shared<TransportState>()) {}

    ~TransportImpl() = default;

    Status registerEndpoint(const std::string &address, Transport::DeliverFn deliver)
    {
        std::lock_guard<std::mutex> lock(state_->mtx_);
        if (state_->endpoints_.count(address))
        {
            return {ErrorCode::InvalidState, "Endpoint already registered"};
        }
        state_->endpoints_[address] = {deliver};
        return {ErrorCode::Ok, ""};
    }

    Status send(const std::string &from, const std::string &to, const Transport::Bytes &data)
    {
        {
            std::lock_guard<std::mutex> lock(state_->mtx_);
            if (!state_->endpoints_.count(to))
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
                    "unknown", // message type - could be inferred from data
                    data.size(),
                    "random_drop");
            }
            return {ErrorCode::NetworkDrop, "Packet dropped by network"};
        }

        // Capture state by shared_ptr to extend its lifetime
        auto state = state_;
        auto params = params_; // capture params by value

        // Simulate latency and deliver asynchronously using detached thread
        std::thread([state, params, to, data]()
                    {
            std::this_thread::sleep_for(params.latency);
            Transport::DeliverFn deliver;
            {
                std::lock_guard<std::mutex> lock(state->mtx_);
                auto it = state->endpoints_.find(to);
                if (it == state->endpoints_.end()) return;
                deliver = it->second.deliver;
            }
            std::cout<<"==================> here 1"<<std::endl;
            if (deliver) deliver(data);
            std::cout << "==================> here 2"<<std::endl; })
            .detach();

        return {ErrorCode::Ok, ""};
    }

    void setParams(NetworkParams p)
    {
        // This is not fully thread-safe if called concurrently with send,
        // but for the simulation, we assume it's called during setup.
        // A more robust implementation would need a lock here.
        params_ = p;
    }

    Status unregisterEndpoint(const std::string &address)
    {
        std::lock_guard<std::mutex> lock(state_->mtx_);
        if (state_->endpoints_.erase(address) == 0)
        {
            return {ErrorCode::NotFound, "Endpoint not registered"};
        }
        return {ErrorCode::Ok, ""};
    }

private:
    NetworkParams params_;
    std::mt19937 rng_;
    DetailedLogger *detailedLogger_;
    std::shared_ptr<TransportState> state_;
};

// Implementation forwarding to TransportImpl

Transport::Transport(unsigned seed, NetworkParams params, DetailedLogger *detailedLogger)
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

Status Transport::unregisterEndpoint(const std::string &address)
{
    return impl_->unregisterEndpoint(address);
}
