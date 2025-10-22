// core/Blockchain.h
// Represents one chain: ledger state, mempool, router, channels.
#pragma once
#include <memory>
#include <vector>
#include "Block.h"
#include "Mempool.h"
#include "EventBus.h"
#include "ibc/IBCRouter.h"
#include "ibc/IBCChannel.h"
#include "util/Logger.h"
#include "util/Metrics.h"

// Forward declaration
class DetailedLogger;

class Blockchain
{
public:
    explicit Blockchain(const std::string &chainId, EventBus &bus, Logger &log, MetricsSink &metrics, DetailedLogger* detailedLogger = nullptr);
    const std::string &id() const;

    // IBC primitives
    Status openChannel(PortId port, ChannelId chan);
    Status closeChannel(PortId port, ChannelId chan);
    Result<IBCPacket> sendIBC(PortId port, ChannelId chan,
                              const std::string &dstChain, PortId dstPort,
                              ChannelId dstChan, const std::string &payload);
    Status onIBCPacket(const IBCPacket &pkt);
    Status onIBCAck(const IBCPacket &ack);

    // Ledger state
    const Block &head() const;
    Status appendBlock(const Block &blk);

    // Node registration (nodes drive consensus)
    void registerNodeId(const std::string &nodeId);

    // Accessors
    Mempool &mempool();
    IBCRouter &router();

private:
    std::string chainId_;
    std::vector<Block> chain_;
    Mempool mempool_;
    IBCRouter router_;
    EventBus &bus_;
    Logger &log_;
    MetricsSink &metrics_;
    DetailedLogger* detailedLogger_;
    std::vector<std::string> nodeIds_;
};
