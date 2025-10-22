// sim/SimulationController.h
// Wires chains, nodes, channels, relayers and runs the scenario.
#pragma once
#include <memory>
#include <vector>
#include "config/ChainConfig.h"
#include "config/SimulationConfig.h"
#include "core/Blockchain.h"
#include "core/Node.h"
#include "ibc/Relayer.h"
#include "net/Transport.h"
#include "core/EventBus.h"
#include "util/Logger.h"
#include "util/Metrics.h"

class SimulationController
{
public:
    SimulationController(const std::vector<ChainConfig> &chains, const SimulationConfig &simCfg);

    Status init(); // builds chains, nodes, and network
    Status openIBC(const std::string &a, PortId ap, ChannelId ac,
                   const std::string &b, PortId bp, ChannelId bc);
    Status start();       // starts all node threads
    void stop();          // stops everything and joins
    void injectTraffic(); // optional workload generator
    void run();           // blocking run until time budget elapses

private:
    std::vector<ChainConfig> chainCfgs_;
    SimulationConfig simCfg_;
    EventBus bus_;
    Logger rootLog_;
    MetricsSink metrics_;
    NetworkParams netParams_;
    Transport transport_;
    std::vector<std::unique_ptr<Blockchain>> chains_;
    std::vector<std::unique_ptr<Node>> nodes_;
    std::unique_ptr<Relayer> relayer_;
    // Mapping helpers
    Blockchain *findChain(const std::string &id);
};
