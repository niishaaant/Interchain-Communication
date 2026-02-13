#include "Blockchain.h"
#include "ibc/IBCTypes.h"
#include "util/DetailedLogger.h"
#include <mutex>
#include <algorithm>

// Anonymous namespace for internal mutex
namespace
{
    std::mutex &getChainMutex()
    {
        static std::mutex mtx;
        return mtx;
    }
}

Blockchain::Blockchain(const std::string &chainId, EventBus &bus, Logger &log, MetricsSink &metrics, DetailedLogger* detailedLogger)
    : chainId_(chainId),
      mempool_(),
      router_(),
      bus_(bus),
      log_(log),
      metrics_(metrics),
      detailedLogger_(detailedLogger)
{
    // Optionally, initialize with a genesis block
    Block genesis;
    genesis.header.chainId = chainId_;
    genesis.header.height = 0;
    chain_.push_back(genesis);
    log_.info("Blockchain " + chainId_ + " initialized with genesis block.");
}

const std::string &Blockchain::id() const
{
    return chainId_;
}

std::string Blockchain::makeChannelKey(const PortId& port, const ChannelId& chan)
{
    return port.value + ":" + chan.value;
}

IBCChannel* Blockchain::getOrCreateChannel(const PortId& port, const ChannelId& chan)
{
    std::lock_guard<std::mutex> lock(channelsMtx_);
    std::string key = makeChannelKey(port, chan);

    auto it = channels_.find(key);
    if (it != channels_.end()) {
        return it->second.get();
    }

    // Create new channel
    auto channel = std::make_unique<IBCChannel>(chainId_, port, chan);
    auto* channelPtr = channel.get();
    channels_[key] = std::move(channel);

    log_.info("Created new IBC channel: " + key);
    return channelPtr;
}

Status Blockchain::openChannel(PortId port, ChannelId chan)
{
    std::lock_guard<std::mutex> lock(getChainMutex());

    // Bind in router
    Status s = router_.bind(port, chan);
    if (!s.ok())
    {
        log_.warn("Failed to bind channel in router: " + s.message);
        return s;
    }

    // Get or create the persistent channel
    IBCChannel* channel = getOrCreateChannel(port, chan);

    // Open the channel
    Status openStatus = channel->open();
    if (!openStatus.ok() && openStatus.code != ErrorCode::InvalidState)
    {
        log_.warn("Failed to open channel: " + openStatus.message);
        return openStatus;
    }

    log_.info("Channel opened and bound: port=" + port.value + " chan=" + chan.value);
    return {ErrorCode::Ok, ""};
}

Status Blockchain::closeChannel(PortId port, ChannelId chan)
{
    std::lock_guard<std::mutex> lock(getChainMutex());
    Status s = router_.unbind(port, chan);
    if (s.ok())
    {
        log_.info("Channel closed: port=" + port.value + " chan=" + chan.value);
    }
    else
    {
        log_.warn("Failed to close channel: " + s.message);
    }
    return s;
}

Result<IBCPacket> Blockchain::sendIBC(PortId port, ChannelId chan,
                                      const std::string &dstChain, PortId dstPort,
                                      ChannelId dstChan, const std::string &payload)
{
    std::lock_guard<std::mutex> lock(getChainMutex());

    // Get or create the persistent channel
    IBCChannel* channel = getOrCreateChannel(port, chan);

    // Ensure channel is open
    Status openStatus = channel->open();
    if (!openStatus.ok() && openStatus.code != ErrorCode::InvalidState)
    {
        return {openStatus, std::nullopt};
    }

    // Make packet using persistent channel
    auto pktRes = channel->makePacket(dstChain, dstPort, dstChan, payload);
    if (!pktRes.status.ok())
    {
        log_.warn("Failed to make IBC packet: " + pktRes.status.message);
        return pktRes;
    }
    // Publish event with serialized packet data
    std::string packetData = serializeIBCPacket(pktRes.value.value());
    Event e{EventKind::IBCPacketSend, chainId_, "", packetData};
    bus_.publish(e);
    metrics_.incCounter("ibc_packets_sent");

    // Detailed IBC event logging
    if (detailedLogger_)
    {
        const IBCPacket& pkt = pktRes.value.value();
        detailedLogger_->logIBCEvent(
            IBCEventType::PacketCreated,
            pkt.srcChain,
            pkt.dstChain,
            pkt.srcPort.value,
            pkt.srcChannel.value,
            pkt.dstPort.value,
            pkt.dstChannel.value,
            pkt.sequence,
            pkt.payload
        );
    }

    return pktRes;
}

Status Blockchain::onIBCPacket(const IBCPacket &pkt)
{
    std::lock_guard<std::mutex> lock(getChainMutex());

    // Get or create the persistent channel for receiving
    IBCChannel* channel = getOrCreateChannel(pkt.dstPort, pkt.dstChannel);

    // Ensure channel is open (auto-open for receiving)
    Status openStatus = channel->open();
    if (!openStatus.ok() && openStatus.code != ErrorCode::InvalidState)
    {
        return openStatus;
    }

    // Accept packet on persistent channel
    Status s = channel->acceptPacket(pkt);
    if (s.ok())
    {
        Event e{EventKind::IBCPacketRecv, chainId_, "", "IBC packet received"};
        bus_.publish(e);
        metrics_.incCounter("ibc_packets_received");

        // Detailed IBC event logging
        if (detailedLogger_)
        {
            detailedLogger_->logIBCEvent(
                IBCEventType::PacketReceived,
                pkt.srcChain,
                pkt.dstChain,
                pkt.srcPort.value,
                pkt.srcChannel.value,
                pkt.dstPort.value,
                pkt.dstChannel.value,
                pkt.sequence,
                pkt.payload
            );
        }

        // Generate and publish acknowledgement
        IBCPacket ack;
        ack.type = IBCPacketType::Ack;
        ack.srcChain = pkt.dstChain;  // We are now the sender
        ack.dstChain = pkt.srcChain;  // Original sender
        ack.srcPort = pkt.dstPort;
        ack.srcChannel = pkt.dstChannel;
        ack.dstPort = pkt.srcPort;
        ack.dstChannel = pkt.srcChannel;
        ack.sequence = pkt.sequence;
        ack.payload = "ack_" + std::to_string(pkt.sequence);

        std::string ackData = serializeIBCPacket(ack);
        Event ackEvent{EventKind::IBCAckSend, chainId_, "", ackData};
        bus_.publish(ackEvent);
        log_.debug("Generated ack for packet seq=" + std::to_string(pkt.sequence));

        // Detailed IBC event logging for ack generation
        if (detailedLogger_)
        {
            detailedLogger_->logIBCEvent(
                IBCEventType::AckGenerated,
                ack.srcChain,
                ack.dstChain,
                ack.srcPort.value,
                ack.srcChannel.value,
                ack.dstPort.value,
                ack.dstChannel.value,
                ack.sequence,
                ack.payload
            );
        }
    }
    else
    {
        log_.warn("Failed to accept IBC packet: " + s.message);
    }
    return s;
}

Status Blockchain::onIBCAck(const IBCPacket &ack)
{
    std::lock_guard<std::mutex> lock(getChainMutex());
    // For demo, just log and publish event
    Event e{EventKind::IBCAckRecv, chainId_, "", "IBC ack received"};
    bus_.publish(e);
    metrics_.incCounter("ibc_acks_received");
    log_.info("IBC ack received for seq=" + std::to_string(ack.sequence));

    // Detailed IBC event logging
    if (detailedLogger_)
    {
        detailedLogger_->logIBCEvent(
            IBCEventType::AckReceived,
            ack.srcChain,
            ack.dstChain,
            ack.srcPort.value,
            ack.srcChannel.value,
            ack.dstPort.value,
            ack.dstChannel.value,
            ack.sequence,
            ack.payload
        );
    }

    return {ErrorCode::Ok, "Ack processed"};
}

const Block &Blockchain::head() const
{
    std::lock_guard<std::mutex> lock(getChainMutex());
    return chain_.back();
}

Status Blockchain::appendBlock(const Block &blk)
{
    std::lock_guard<std::mutex> lock(getChainMutex());
    if (!chain_.empty() && blk.header.height != chain_.back().header.height + 1)
    {
        log_.warn("Block height mismatch: got " + std::to_string(blk.header.height) +
                  ", expected " + std::to_string(chain_.back().header.height + 1));
        return {ErrorCode::InvalidState, "Block height mismatch"};
    }
    chain_.push_back(blk);
    Event e{EventKind::BlockFinalized, chainId_, "", "Block appended at height " + std::to_string(blk.header.height)};
    bus_.publish(e);
    metrics_.incCounter("blocks_appended");
    log_.info("Block appended at height " + std::to_string(blk.header.height));
    return {ErrorCode::Ok, "Block appended"};
}

void Blockchain::registerNodeId(const std::string &nodeId)
{
    std::lock_guard<std::mutex> lock(getChainMutex());
    if (std::find(nodeIds_.begin(), nodeIds_.end(), nodeId) == nodeIds_.end())
    {
        nodeIds_.push_back(nodeId);
        log_.info("Node registered: " + nodeId);
    }
}

Mempool &Blockchain::mempool()
{
    return mempool_;
}

IBCRouter &Blockchain::router()
{
    return router_;
}