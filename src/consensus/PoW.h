// consensus/PoW.h
// Simplified PoW engine (nonce search simulated).
#pragma once
#include "Consensus.h"
#include <memory>

class PoWImpl;

class PoW final : public Consensus
{
public:
    explicit PoW(uint32_t difficulty, MetricsSink& metrics);
    ~PoW();
    Result<Block> propose(const ConsensusContext &ctx,
                          const std::vector<Transaction> &txs,
                          const Block &prev) override;
    Status onRemoteBlock(const Block &blk) override;
    bool isFinal(const Block &blk) const override;
    std::string name() const override;

private:
    uint32_t difficulty_;
    std::unique_ptr<PoWImpl> pImpl_;
};
