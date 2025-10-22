// config/SimulationConfig.h
// Global knobs for transport, failure rates, run duration.
#pragma once
#include <chrono>

struct SimulationConfig
{
    std::chrono::milliseconds defaultLinkLatency{50};
    double packetDropRate{0.01};
    std::chrono::milliseconds runFor{std::chrono::minutes(2)};
    unsigned rngSeed{42};
};
