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

    // Traffic generation parameters
    std::chrono::milliseconds trafficGenInterval{100};  // Average time between transactions
    double ibcTrafficRatio{0.3};  // 30% of traffic is IBC, 70% is regular
    bool enableContinuousTraffic{true};  // Enable/disable continuous traffic

    // Detailed logging parameters
    bool enableDetailedTransactionLogs{true};
    bool enableIBCEventLogs{true};
    bool enableNodeStateSnapshots{true};
    bool enableNetworkDropLogs{true};
    bool enableRelayerStateLogs{true};

    // Relayer configuration
    size_t relayerCount{3};  // Number of concurrent relayers
    bool enableRelayerCompetition{true};  // If false, use round-robin assignment
};
