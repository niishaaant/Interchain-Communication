// filepath: /home/niishaaant/work/blockchain-comm-sim/src/ibc/IBCRouter.cpp

#include "IBCRouter.h"
#include <mutex>
#include <algorithm>

// Bind ports to channels
Status IBCRouter::bind(PortId port, ChannelId chan)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Key key{port.value, chan.value};
    auto it = std::find_if(bindings_.begin(), bindings_.end(),
                           [&](const Key &k)
                           { return k.p == key.p && k.c == key.c; });
    if (it != bindings_.end())
    {
        return Status{ErrorCode::InvalidState, "Binding already exists"};
    }
    bindings_.push_back(key);
    return Status{ErrorCode::Ok, "Bound successfully"};
}

Status IBCRouter::unbind(PortId port, ChannelId chan)
{
    std::lock_guard<std::mutex> lock(mutex_);
    Key key{port.value, chan.value};
    auto it = std::find_if(bindings_.begin(), bindings_.end(),
                           [&](const Key &k)
                           { return k.p == key.p && k.c == key.c; });
    if (it == bindings_.end())
    {
        return Status{ErrorCode::NotFound, "Binding not found"};
    }
    bindings_.erase(it);
    return Status{ErrorCode::Ok, "Unbound successfully"};
}

bool IBCRouter::isBound(PortId port, ChannelId chan) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    Key key{port.value, chan.value};
    auto it = std::find_if(bindings_.begin(), bindings_.end(),
                           [&](const Key &k)
                           { return k.p == key.p && k.c == key.c; });
    return it != bindings_.end();
}