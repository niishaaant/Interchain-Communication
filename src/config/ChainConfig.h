// config/ChainConfig.h
// Per-chain parameters and consensus selection.
#pragma once
#include <string>
#include <chrono>

enum class ConsensusKind
{
    PoW,
    PoS,
    PBFT
};

struct ChainConfig
{
    std::string chainId;
    ConsensusKind consensusKind{ConsensusKind::PoW};
    size_t nodeCount{4};
    std::chrono::milliseconds blockTime{1000};
    // PoW/PoS/PBFT-specific knobs (difficulty, validator set size, f, etc.)
    uint32_t powDifficulty{4};
    size_t validatorSetSize{4};
    size_t pbftFaultTolerance{1}; // f
};
