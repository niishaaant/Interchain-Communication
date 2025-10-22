// filepath: /home/niishaaant/work/blockchain-comm-sim/src/consensus/PoW.cpp
#include "PoW.h"
#include "util/Metrics.h" // Added
#include <chrono>
#include <random>
#include <sstream>
#include <thread>
#include <mutex>
#include <iomanip>
#include <unordered_set>

// Internal PoW implementation
class PoWImpl
{
public:
    explicit PoWImpl(uint32_t difficulty, MetricsSink& metrics)
        : difficulty_(difficulty), metrics_(metrics)
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

        metrics_.incCounter("block_proposed_PoW"); // Added metric

        // Simulate PoW by searching for a nonce that produces a hash with enough leading zeros
        uint64_t nonce = 0;
        std::string hash;
        while (true)
        {
            hash = computeBlockHash(block, nonce);
            if (hasLeadingZeros(hash, difficulty_))
                break;
            ++nonce;
            // For simulation, avoid infinite loop
            if (nonce > 1000000)
                return {{ErrorCode::ConsensusFault, "PoW: nonce search failed"}, {}};
        }

        block.extra = std::to_string(nonce);
        minedBlocks_.insert(blockId(block, nonce));
        metrics_.incCounter("block_finalized_PoW"); // Added metric
        return {{ErrorCode::Ok, ""}, block};
    }

    Status onRemoteBlock(const Block &blk)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_.incCounter("block_received_PoW"); // Added metric
        // Accept remote block if PoW is valid
        uint64_t nonce = 0;
        try
        {
            nonce = std::stoull(blk.extra);
        }
        catch (...)
        {
            return {ErrorCode::InvalidState, "PoW: invalid nonce in extra"};
        }
        std::string hash = computeBlockHash(blk, nonce);
        if (!hasLeadingZeros(hash, difficulty_))
            return {ErrorCode::ConsensusFault, "PoW: invalid PoW"};
        minedBlocks_.insert(blockId(blk, nonce));
        metrics_.incCounter("block_finalized_PoW"); // Added metric
        return {ErrorCode::Ok, ""};
    }

    bool isFinal(const Block &blk) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t nonce = 0;
        try
        {
            nonce = std::stoull(blk.extra);
        }
        catch (...)
        {
            return false;
        }
        return minedBlocks_.count(blockId(blk, nonce)) > 0;
    }

    std::string name() const { return "PoW"; }

private:
    uint32_t difficulty_;
    mutable std::mutex mutex_;
    std::unordered_set<std::string> minedBlocks_;
    MetricsSink& metrics_; // Added

    // Dummy block id: chainId:height:prevHash:nonce
    std::string blockId(const Block &blk, uint64_t nonce) const
    {
        std::ostringstream oss;
        oss << blk.header.chainId << ":" << blk.header.height << ":" << blk.header.prevHash << ":" << nonce;
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

    // Simulate block hash as hex string
    std::string computeBlockHash(const Block &blk, uint64_t nonce) const
    {
        std::ostringstream oss;
        oss << blk.header.chainId << blk.header.height << blk.header.prevHash
            << blk.header.stateRoot << nonce;
        std::hash<std::string> hasher;
        size_t h = hasher(oss.str());
        std::ostringstream hex;
        hex << std::hex << std::setw(16) << std::setfill('0') << h;
        return hex.str();
    }

    // Check if hash has enough leading zeros (in hex)
    bool hasLeadingZeros(const std::string &hash, uint32_t zeros) const
    {
        for (uint32_t i = 0; i < zeros; ++i)
        {
            if (i >= hash.size() || hash[i] != '0')
                return false;
        }
        return true;
    }
};

// PoW class implementation

PoW::PoW(uint32_t difficulty, MetricsSink& metrics)
    : difficulty_(difficulty), pImpl_(std::make_unique<PoWImpl>(difficulty, metrics))
{
}

PoW::~PoW() = default;

Result<Block> PoW::propose(const ConsensusContext &ctx,
                           const std::vector<Transaction> &txs,
                           const Block &prev)
{
    return pImpl_->propose(ctx, txs, prev);
}

Status PoW::onRemoteBlock(const Block &blk)
{
    return pImpl_->onRemoteBlock(blk);
}

bool PoW::isFinal(const Block &blk) const
{
    return pImpl_->isFinal(blk);
}

std::string PoW::name() const
{
    return pImpl_->name();
}