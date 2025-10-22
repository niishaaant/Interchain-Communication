// consensus/PoS.h
// Simplified PoS with validator signatures.
#pragma once
#include "Consensus.h"
#include <memory>

class PoSImpl;

class PoS final : public Consensus
{
public:
    explicit PoS(size_t validatorSetSize, MetricsSink& metrics);
    ~PoS();
    Result<Block> propose(const ConsensusContext &ctx,
                          const std::vector<Transaction> &txs,
                          const Block &prev) override;
    Status onRemoteBlock(const Block &blk) override;
    bool isFinal(const Block &blk) const override;
    std::string name() const override;

private:
    size_t validators_;
    std::unique_ptr<PoSImpl> pImpl_;
};
