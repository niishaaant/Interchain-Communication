#include <iostream>
#include <vector>
#include <csignal>
#include <atomic>
#include <chrono>
#include "sim/SimulationController.h"
#include "config/ChainConfig.h"
#include "config/SimulationConfig.h"
#include "util/Logger.h"
#include "util/Error.h"

static std::atomic<bool> g_stop{false};

static void handleSignal(int)
{
    g_stop.store(true);
}

int main(int argc, char **argv)
{
    // Install simple SIGINT handler to stop gracefully
    std::signal(SIGINT, handleSignal);

    // Configure simulation-wide parameters
    SimulationConfig simCfg;
    simCfg.defaultLinkLatency = std::chrono::milliseconds(50);
    simCfg.packetDropRate = 0.01;
    simCfg.runFor = std::chrono::minutes(2);
    simCfg.rngSeed = 42;

    // Prepare simple chain topology with different consensus kinds
    std::vector<ChainConfig> chains;

    ChainConfig c1;
    c1.chainId = "chain-A";
    c1.consensusKind = ConsensusKind::PoW;
    c1.nodeCount = 3;
    c1.blockTime = std::chrono::milliseconds(1000);
    c1.powDifficulty = 3;
    chains.push_back(c1);

    ChainConfig c2;
    c2.chainId = "chain-B";
    c2.consensusKind = ConsensusKind::PoS;
    c2.nodeCount = 4;
    c2.blockTime = std::chrono::milliseconds(800);
    c2.validatorSetSize = 4;
    chains.push_back(c2);

    ChainConfig c3;
    c3.chainId = "chain-C";
    c3.consensusKind = ConsensusKind::PBFT;
    c3.nodeCount = 4;
    c3.blockTime = std::chrono::milliseconds(500);
    c3.pbftFaultTolerance = 1;
    chains.push_back(c3);

    // Root logger used by SimulationController (name shown in logs)
    Logger rootLog("sim");

    // Construct controller
    SimulationController controller(chains, simCfg);

    // Initialize simulation (build chains, nodes, network)
    Status s = controller.init();
    if (!s.ok())
    {
        std::cerr << "Simulation init failed: " << s.message << std::endl;
        return 1;
    }

    // Start nodes
    s = controller.start();
    if (!s.ok())
    {
        std::cerr << "Simulation start failed: " << s.message << std::endl;
        return 2;
    }

    // Open IBC channels (example)
    controller.openIBC("chain-A", {"port-A"}, {"channel-A"},
                       "chain-B", {"port-B"}, {"channel-B"});

    // Inject traffic
    controller.injectTraffic();

    std::cout << "Simulation running (press Ctrl-C to stop early)..." << std::endl;

    // Run blocking until time budget elapses or user signals stop
    // If SimulationController::run blocks for its configured duration, this will return when done.
    // We still support early shutdown via SIGINT below.
    std::thread runThread([&controller]() {
        controller.run();
    });

    // Wait for user interrupt or run completion
    while (!g_stop.load())
    {
        // Poll every 200ms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "Stopping simulation..." << std::endl;
    controller.stop();

    if (runThread.joinable())
        runThread.join();

    std::cout << "Simulation stopped." << std::endl;
    return 0;
}