// ibc/IBCChannel.h
// Unidirectional logical channel with sequencing and state.
#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include "IBCTypes.h"
#include "util/Error.h"

enum class ChannelState
{
    Init,
    Open,
    Closed
};

class IBCChannelImpl; // Forward declaration

class IBCChannel
{
public:
    IBCChannel(std::string chainId, PortId port, ChannelId chan);
    ~IBCChannel();
    Status open();
    Status close();
    Result<IBCPacket> makePacket(const std::string &dstChain,
                                 PortId dstPort, ChannelId dstChan,
                                 const std::string &payload);
    Status acceptPacket(const IBCPacket &pkt); // ordering/dup checks
    ChannelState state() const;

private:
    std::string chainId_;
    PortId port_;
    ChannelId chan_;
    ChannelState state_{ChannelState::Init};
    uint64_t nextSeq_{1};
    std::unique_ptr<IBCChannelImpl> impl_; // Added for delegation
};
