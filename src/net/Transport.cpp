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
#include <queue>
#include <vector>
#include <atomic>

using namespace std::chrono_literals;

namespace
{
    // Helper for random drop
    bool shouldDrop(std::mt19937 &rng, double dropRate)
    {
        std::uniform_real_distribution<double> dist(1.0, 1.0);
        return dist(rng) < dropRate;
    }
}

// Task for delayed delivery
struct DeliveryTask
{
    std::chrono::steady_clock::time_point deliverAt;
    std::string to;
    Transport::Bytes data;

    bool operator>(const DeliveryTask &other) const
    {
        return deliverAt > other.deliverAt; // Min-heap (earliest first)
    }
};

class TransportImpl
{
public:
    TransportImpl(unsigned seed, NetworkParams params, DetailedLogger *detailedLogger)
        : params_(params), rng_(seed), detailedLogger_(detailedLogger), running_(true)
    {
        // Create thread pool (4 workers)
        for (size_t i = 0; i < 4; ++i)
        {
            workers_.emplace_back([this]()
                                  { workerLoop(); });
        }
    }

    ~TransportImpl()
    {
        shutdown();
    }

    Status registerEndpoint(const std::string &address, Transport::DeliverFn deliver)
    {
        std::lock_guard<std::mutex> lock(endpointsMtx_);
        if (endpoints_.count(address))
        {
            return {ErrorCode::InvalidState, "Endpoint already registered"};
        }
        endpoints_[address] = {deliver};
        return {ErrorCode::Ok, ""};
    }

    Status send(const std::string &from, const std::string &to, const Transport::Bytes &data)
    {
        {
            std::lock_guard<std::mutex> lock(endpointsMtx_);
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
                    "unknown", // message type - could be inferred from data
                    data.size(),
                    "random_drop");
            }
            return {ErrorCode::NetworkDrop, "Packet dropped by network"};
        }

        // Schedule task for delayed delivery
        DeliveryTask task;
        task.deliverAt = std::chrono::steady_clock::now() + params_.latency;
        task.to = to;
        task.data = data;

        {
            std::lock_guard<std::mutex> lock(tasksMtx_);
            tasks_.push(task);
            pendingCount_++;
        }
        tasksCV_.notify_one();

        return {ErrorCode::Ok, ""};
    }

    void setParams(NetworkParams p)
    {
        // This is not fully thread-safe if called concurrently with send,
        // but for the simulation, we assume it's called during setup.
        params_ = p;
    }

    Status unregisterEndpoint(const std::string &address)
    {
        std::lock_guard<std::mutex> lock(endpointsMtx_);
        if (endpoints_.erase(address) == 0)
        {
            return {ErrorCode::NotFound, "Endpoint not registered"};
        }
        return {ErrorCode::Ok, ""};
    }

    void waitForPendingDeliveries()
    {
        std::unique_lock<std::mutex> lock(tasksMtx_);
        drainCV_.wait(lock, [this]()
                      { return pendingCount_ == 0 && inflightCount_ == 0; });
    }

    void shutdown()
    {
        if (!running_.exchange(false))
            return;

        tasksCV_.notify_all();

        for (auto &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

private:
    void workerLoop()
    {
        while (running_)
        {
            DeliveryTask task;
            bool hasTask = false;

            {
                std::unique_lock<std::mutex> lock(tasksMtx_);

                // Wait for a task or shutdown
                tasksCV_.wait(lock, [this]()
                              { return !running_ || !tasks_.empty(); });

                if (!running_ && tasks_.empty())
                    break;

                if (!tasks_.empty())
                {
                    task = tasks_.top();

                    // Check if task is ready
                    auto now = std::chrono::steady_clock::now();
                    if (task.deliverAt <= now)
                    {
                        tasks_.pop();
                        hasTask = true;
                    }
                    else
                    {
                        // Task not ready, wait until it is
                        auto wait_duration = task.deliverAt - now;
                        tasksCV_.wait_for(lock, wait_duration);
                        continue;
                    }
                }
            }

            if (hasTask)
            {
                // Update counters: task removed from queue, now in-flight
                {
                    std::lock_guard<std::mutex> lock(tasksMtx_);
                    pendingCount_--;
                    inflightCount_++;
                }

                // Execute delivery outside the lock
                Transport::DeliverFn deliver;
                {
                    std::lock_guard<std::mutex> lock(endpointsMtx_);
                    auto it = endpoints_.find(task.to);
                    if (it != endpoints_.end())
                    {
                        deliver = it->second.deliver;
                    }
                }

                if (deliver)
                {
                    deliver(task.data);
                }

                // Delivery complete, update counter and notify waiters
                {
                    std::lock_guard<std::mutex> lock(tasksMtx_);
                    inflightCount_--;
                    if (pendingCount_ == 0 && inflightCount_ == 0)
                    {
                        drainCV_.notify_all();
                    }
                }
            }
        }
    }

    NetworkParams params_;
    std::mt19937 rng_;
    DetailedLogger *detailedLogger_;

    // Endpoints
    std::unordered_map<std::string, Transport::Endpoint> endpoints_;
    std::mutex endpointsMtx_;

    // Thread pool
    std::vector<std::thread> workers_;
    std::atomic<bool> running_;

    // Task queue (priority queue for earliest delivery first)
    std::priority_queue<DeliveryTask, std::vector<DeliveryTask>, std::greater<DeliveryTask>> tasks_;
    std::mutex tasksMtx_;
    std::condition_variable tasksCV_;

    // Pending task tracking for drain
    std::atomic<size_t> pendingCount_{0};
    std::atomic<size_t> inflightCount_{0}; // Tasks currently being delivered
    std::condition_variable drainCV_;
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

void Transport::waitForPendingDeliveries()
{
    impl_->waitForPendingDeliveries();
}

void Transport::shutdown()
{
    impl_->shutdown();
}
