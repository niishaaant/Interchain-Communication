// consensus/ConsensusFactory.h
// Factory to instantiate consensus per ChainConfig.
#pragma once
#include <memory>
#include "Consensus.h"
#include "config/ChainConfig.h"

struct ConsensusFactory
{
    static std::unique_ptr<Consensus> make(const ChainConfig &cfg, MetricsSink& metrics);
};
