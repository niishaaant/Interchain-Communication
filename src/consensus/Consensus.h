// consensus/Consensus.h
// Strategy interface for pluggable consensus engines.
#pragma once
#include <optional>
#include <vector>
#include <string>
#include "util/Error.h"
#include "core/Block.h"
#include "core/Transaction.h"

class MetricsSink;

struct ConsensusContext
{
    std::string chainId;
    std::string nodeId;
    uint64_t currentHeight{0};
};

class Consensus
{
public:
    virtual ~Consensus() = default;

    // Called by Node to attempt proposing/producing a block.
    virtual Result<Block> propose(const ConsensusContext &ctx,
                                  const std::vector<Transaction> &txs,
                                  const Block &prev) = 0;

    // Called when remote block/round info is received.
    virtual Status onRemoteBlock(const Block &blk) = 0;

    // Whether a given block is finalized/committed under this consensus.
    virtual bool isFinal(const Block &blk) const = 0;

    // Short name for logging/metrics.
    virtual std::string name() const = 0;
};
