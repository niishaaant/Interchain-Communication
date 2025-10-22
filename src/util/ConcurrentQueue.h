#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>

template <typename T>
class ConcurrentQueue
{
public:
    void push(T item)
    {
        std::unique_lock<std::mutex> lock(m_);
        if (closed_)
        {
            throw std::runtime_error("Queue is closed");
        }
        q_.push(std::move(item));
        cv_.notify_one();
    }

    std::optional<T> tryPop()
    {
        std::unique_lock<std::mutex> lock(m_);
        if (q_.empty())
        {
            return std::nullopt;
        }
        T item = std::move(q_.front());
        q_.pop();
        return item;
    }

    T waitPop()
    {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this]
                 { return closed_ || !q_.empty(); });
        if (q_.empty())
        {
            throw std::runtime_error("Queue is closed and empty");
        }
        T item = std::move(q_.front());
        q_.pop();
        return item;
    }

    void close()
    {
        std::unique_lock<std::mutex> lock(m_);
        closed_ = true;
        cv_.notify_all();
    }

    bool closed() const
    {
        std::unique_lock<std::mutex> lock(m_);
        return closed_;
    }

private:
    std::queue<T> q_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    bool closed_{false};
};