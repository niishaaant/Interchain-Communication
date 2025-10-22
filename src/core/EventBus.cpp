
#include "EventBus.h"
#include <algorithm>
#include <mutex>

namespace
{
    std::mutex &getBusMutex()
    {
        static std::mutex mtx;
        return mtx;
    }
}

int EventBus::subscribe(EventKind kind, Handler h)
{
    std::lock_guard<std::mutex> lock(getBusMutex());
    int token = nextToken_++;
    subs_[kind].emplace_back(token, std::move(h));
    return token;
}

void EventBus::unsubscribe(int token)
{
    std::lock_guard<std::mutex> lock(getBusMutex());
    for (auto &[kind, vec] : subs_)
    {
        auto it = std::remove_if(vec.begin(), vec.end(),
                                 [token](const std::pair<int, Handler> &p)
                                 { return p.first == token; });
        if (it != vec.end())
        {
            vec.erase(it, vec.end());
            break;
        }
    }
}

void EventBus::publish(const Event &e)
{
    std::lock_guard<std::mutex> lock(getBusMutex());
    auto it = subs_.find(e.kind);
    if (it != subs_.end())
    {
        auto handlers = it->second;
        for (const auto &[token, handler] : handlers)
        {
            handler(e);
        }
    }
}