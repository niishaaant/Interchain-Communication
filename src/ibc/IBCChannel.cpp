// filepath: /home/niishaaant/work/blockchain-comm-sim/src/ibc/IBCChannel.cpp

#include "IBCChannel.h"
#include <mutex>

// For thread safety
class IBCChannelImpl
{
public:
    IBCChannelImpl(std::string chainId, PortId port, ChannelId chan)
        : chainId_(std::move(chainId)), port_(std::move(port)), chan_(std::move(chan)), state_(ChannelState::Init), nextSeq_(1) {}

    Status open()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_ == ChannelState::Closed)
        {
            return {ErrorCode::ChannelClosed, "Channel is closed"};
        }
        if (state_ == ChannelState::Open)
        {
            return {ErrorCode::InvalidState, "Channel already open"};
        }
        state_ = ChannelState::Open;
        return {ErrorCode::Ok, "Channel opened"};
    }

    Status close()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_ == ChannelState::Closed)
        {
            return {ErrorCode::ChannelClosed, "Channel already closed"};
        }
        state_ = ChannelState::Closed;
        return {ErrorCode::Ok, "Channel closed"};
    }

    Result<IBCPacket> makePacket(const std::string &dstChain,
                                 PortId dstPort, ChannelId dstChan,
                                 const std::string &payload)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_ != ChannelState::Open)
        {
            return {{ErrorCode::InvalidState, "Channel not open"}, std::nullopt};
        }
        IBCPacket pkt;
        pkt.type = IBCPacketType::Data;
        pkt.srcChain = chainId_;
        pkt.dstChain = dstChain;
        pkt.srcPort = port_;
        pkt.srcChannel = chan_;
        pkt.dstPort = dstPort;
        pkt.dstChannel = dstChan;
        pkt.sequence = nextSeq_++;
        pkt.payload = payload;
        return {{ErrorCode::Ok, ""}, pkt};
    }

    Status acceptPacket(const IBCPacket &pkt)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_ != ChannelState::Open)
        {
            return {ErrorCode::ChannelClosed, "Channel not open"};
        }
        // Check for correct sequencing
        if (pkt.sequence != nextSeq_)
        {
            return {ErrorCode::InvalidState, "Packet sequence mismatch"};
        }
        // Accept the packet and increment sequence
        nextSeq_++;
        return {ErrorCode::Ok, "Packet accepted"};
    }

    ChannelState state() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return state_;
    }

private:
    std::string chainId_;
    PortId port_;
    ChannelId chan_;
    mutable std::mutex mtx_;
    ChannelState state_;
    uint64_t nextSeq_;
};

// Implementation delegation
IBCChannel::IBCChannel(std::string chainId, PortId port, ChannelId chan)
    : chainId_(std::move(chainId)), port_(std::move(port)), chan_(std::move(chan)), state_(ChannelState::Init), nextSeq_(1),
      impl_(std::make_unique<IBCChannelImpl>(chainId_, port_, chan_)) {}

IBCChannel::~IBCChannel() = default;

Status IBCChannel::open() { return impl_->open(); }
Status IBCChannel::close() { return impl_->close(); }
Result<IBCPacket> IBCChannel::makePacket(const std::string &dstChain, PortId dstPort, ChannelId dstChan, const std::string &payload)
{
    return impl_->makePacket(dstChain, dstPort, dstChan, payload);
}
Status IBCChannel::acceptPacket(const IBCPacket &pkt) { return impl_->acceptPacket(pkt); }
ChannelState IBCChannel::state() const { return impl_->state(); }

// Note: You need to add `#include <mutex>` to your imports for thread safety.
// Also, add a unique_ptr to IBCChannelImpl in your IBCChannel class definition for this delegation pattern.