// ibc/IBCRouter.h
// Demultiplexes incoming IBC packets to bound channels/ports.
#pragma once
#include <unordered_map>
#include <vector>
#include <mutex>
#include "IBCTypes.h"
#include "util/Error.h"

class IBCRouter
{
public:
    Status bind(PortId port, ChannelId chan);
    Status unbind(PortId port, ChannelId chan);
    bool isBound(PortId port, ChannelId chan) const;

private:
    struct Key
    {
        std::string p;
        std::string c;
    };
    std::vector<Key> bindings_;
    mutable std::mutex mutex_;
};
