// consensus/ConsensusFactory.cpp
#include "consensus/ConsensusFactory.h"
#include "consensus/PoW.h"
#include "consensus/PoS.h"
#include "consensus/PBFT.h"
#include "util/Metrics.h" // Added
#include <stdexcept>

std::unique_ptr<Consensus> ConsensusFactory::make(const ChainConfig &cfg, MetricsSink &metrics)
{
    switch (cfg.consensusKind)
    {
    case ConsensusKind::PoW:
        return std::make_unique<PoW>(cfg.powDifficulty, metrics);
    case ConsensusKind::PoS:
        return std::make_unique<PoS>(cfg.validatorSetSize, metrics);
    case ConsensusKind::PBFT:
        return std::make_unique<PBFT>(cfg.pbftFaultTolerance, metrics);
    default:
        throw std::runtime_error("Unknown consensus kind in ChainConfig: " + std::to_string(static_cast<int>(cfg.consensusKind)));
    }
}
