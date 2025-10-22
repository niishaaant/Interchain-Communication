// consensus/PBFT.h
// Simplified PBFT-style finality (prepare/commit simulated).
#pragma once
#include "Consensus.h"
#include <memory>

// Forward declaration of PBFTImpl
class PBFTImpl;

class PBFT final : public Consensus
{
public:
    explicit PBFT(size_t f, MetricsSink& metrics);
    ~PBFT(); // Destructor defined in .cpp
    Result<Block> propose(const ConsensusContext &ctx,
                          const std::vector<Transaction> &txs,
                          const Block &prev) override;
    Status onRemoteBlock(const Block &blk) override;
    bool isFinal(const Block &blk) const override;
    std::string name() const override;

private:
    size_t f_; // fault tolerance parameter
    std::unique_ptr<PBFTImpl> pImpl_;
};
