#include "PBFT.h"
#include "util/Metrics.h" // Added
#include <mutex>
#include <unordered_map>
#include <set>
#include <sstream>

// Internal PBFT implementation
class PBFTImpl
{
public:
    explicit PBFTImpl(size_t f, MetricsSink& metrics)
        : f_(f), metrics_(metrics)
    {
    }

    // Simulate prepare/commit for a block
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
        block.header.stateRoot = computeStateRoot(txs); // Dummy hash
        block.txs = txs;
        block.extra = "PBFT:proposed";

        metrics_.incCounter("block_proposed_PBFT"); // Added metric

        // Simulate prepare/commit votes
        auto blkId = blockId(block);
        prepareVotes_[blkId].insert(ctx.nodeId);
        commitVotes_[blkId].insert(ctx.nodeId);

        // Mark as finalized if enough commits
        if (commitVotes_[blkId].size() >= quorum())
        {
            finalizedBlocks_.insert(blkId);
            metrics_.incCounter("block_finalized_PBFT"); // Added metric
        }

        return {{ErrorCode::Ok, ""}, block};
    }

    Status onRemoteBlock(const Block &blk)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.incCounter("block_received_PBFT"); // Added metric
        auto blkId = blockId(blk);

        // Simulate receiving prepare/commit from remote
        // For demo, just use "remote" as nodeId
        prepareVotes_[blkId].insert("remote");
        commitVotes_[blkId].insert("remote");

        if (commitVotes_[blkId].size() >= quorum())
        {
            finalizedBlocks_.insert(blkId);
            metrics_.incCounter("block_finalized_PBFT"); // Added metric
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

    std::string name() const { return "PBFT"; }

private:
    size_t f_;
    mutable std::mutex mutex_;

    // Block id -> set of nodeIds that prepared/committed
    std::unordered_map<std::string, std::set<std::string>> prepareVotes_;
    std::unordered_map<std::string, std::set<std::string>> commitVotes_;
    std::set<std::string> finalizedBlocks_;
    MetricsSink& metrics_; // Added

    size_t quorum() const { return 2 * f_ + 1; }

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

// PBFT class implementation

PBFT::PBFT(size_t f, MetricsSink& metrics)
    : f_(f), pImpl_(std::make_unique<PBFTImpl>(f, metrics))
{
}

PBFT::~PBFT() = default;

Result<Block> PBFT::propose(const ConsensusContext &ctx,
                            const std::vector<Transaction> &txs,
                            const Block &prev)
{
    return pImpl_->propose(ctx, txs, prev);
}

Status PBFT::onRemoteBlock(const Block &blk)
{
    return pImpl_->onRemoteBlock(blk);
}

bool PBFT::isFinal(const Block &blk) const
{
    return pImpl_->isFinal(blk);
}

std::string PBFT::name() const
{
    return pImpl_->name();
}