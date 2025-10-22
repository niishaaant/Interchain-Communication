#include "PoS.h"
#include "util/Metrics.h" // Added
#include <mutex>
#include <unordered_map>
#include <set>
#include <sstream>

// Internal PoS implementation
class PoSImpl
{
public:
    explicit PoSImpl(size_t validatorSetSize, MetricsSink& metrics)
        : validators_(validatorSetSize), metrics_(metrics)
    {
    }

    Result<Block> propose(const ConsensusContext &ctx,
                          const std::vector<Transaction> &txs,
                          const Block &prev)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Block block;
        block.header.chainId = ctx.chainId;
        block.header.height = prev.header.height + 1;
        block.header.prevHash = prev.header.stateRoot; // Simplified: use stateRoot as prevHash
        block.header.timestamp = std::chrono::system_clock::now();
        block.header.stateRoot = computeStateRoot(txs);
        block.txs = txs;
        block.extra = "PoS:proposed:" + ctx.nodeId;

        metrics_.incCounter("block_proposed_PoS"); // Added metric

        // Simulate validator signature
        auto blkId = blockId(block);
        signatures_[blkId].insert(ctx.nodeId);

        // Mark as finalized if enough signatures
        if (signatures_[blkId].size() >= quorum())
        {
            finalizedBlocks_.insert(blkId);
            metrics_.incCounter("block_finalized_PoS"); // Added metric
        }

        return {{ErrorCode::Ok, ""}, block};
    }

    Status onRemoteBlock(const Block &blk)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.incCounter("block_received_PoS"); // Added metric
        auto blkId = blockId(blk);

        // Simulate receiving a signature from a remote validator
        signatures_[blkId].insert("remote");

        if (signatures_[blkId].size() >= quorum())
        {
            finalizedBlocks_.insert(blkId);
            metrics_.incCounter("block_finalized_PoS"); // Added metric
            return {ErrorCode::Ok, ""};
        }
        return {ErrorCode::Ok, ""};
    }

    bool isFinal(const Block &blk) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto blkId = blockId(blk);
        return finalizedBlocks_.count(blkId) > 0;
    }

    std::string name() const { return "PoS"; }

private:
    size_t validators_;
    mutable std::mutex mutex_;

    // Block id -> set of validator ids that signed
    std::unordered_map<std::string, std::set<std::string>> signatures_;
    std::set<std::string> finalizedBlocks_;
    MetricsSink& metrics_; // Added

    size_t quorum() const { return (validators_ * 2) / 3 + 1; }

    // Dummy block id: chainId:height:prevHash
    std::string blockId(const Block &blk) const
    {
        std::ostringstream oss;
        oss << blk.header.chainId << ":" << blk.header.height << ":" << blk.header.prevHash;
        return oss.str();
    }

    // Dummy hash for state root
    std::string computeStateRoot(const std::vector<Transaction> &txs)
    {
        std::hash<std::string> hasher;
        size_t h = 0;
        for (const auto &tx : txs)
        {
            h ^= hasher(tx.from + tx.to + tx.payload);
        }
        return std::to_string(h);
    }
};

// PoS class implementation

PoS::PoS(size_t validatorSetSize, MetricsSink& metrics)
    : validators_(validatorSetSize), pImpl_(std::make_unique<PoSImpl>(validatorSetSize, metrics))
{
}

PoS::~PoS() = default;

Result<Block> PoS::propose(const ConsensusContext &ctx,
                           const std::vector<Transaction> &txs,
                           const Block &prev)
{
    return pImpl_->propose(ctx, txs, prev);
}

Status PoS::onRemoteBlock(const Block &blk)
{
    return pImpl_->onRemoteBlock(blk);
}

bool PoS::isFinal(const Block &blk) const
{
    return pImpl_->isFinal(blk);
}

std::string PoS::name() const
{
    return pImpl_->name();
}