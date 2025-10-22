// core/Block.h
// Block primitives used by all consensuses (extend via metadata).
#pragma once
#include <vector>
#include <chrono>
#include "Types.h"
#include "Transaction.h"

struct BlockHeader
{
    std::string chainId;
    uint64_t height{0};
    Hash prevHash;
    std::chrono::system_clock::time_point timestamp{};
    Hash stateRoot;
};

struct Block
{
    BlockHeader header;
    std::vector<Transaction> txs;
    // Consensus-specific fields serialized into "extra".
    std::string extra; // e.g., PoW nonce, PBFT commits, PoS sigs
};
