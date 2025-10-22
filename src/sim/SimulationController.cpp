#include "sim/SimulationController.h"
#include "consensus/ConsensusFactory.h"
#include "core/Node.h"
#include "core/Blockchain.h"
#include <iostream>
#include <algorithm>

SimulationController::SimulationController(const std::vector<ChainConfig>& chains, const SimulationConfig& simCfg)
    : simCfg_(simCfg),
      rootLog_("sim"),
      metrics_(),
      detailedLogger_(),
      netParams_{simCfg.defaultLinkLatency, simCfg.packetDropRate},
      transport_(simCfg.rngSeed, netParams_, &detailedLogger_),
      trafficRng_(simCfg.rngSeed + 1)  // Different seed for traffic
{
    for (const auto& chainCfg : chains) {
        chainCfgs_.push_back(chainCfg);
    }

    // Configure detailed logger based on simulation config
    detailedLogger_.enableCategory(LogCategory::Transactions, simCfg_.enableDetailedTransactionLogs);
    detailedLogger_.enableCategory(LogCategory::IBCEvents, simCfg_.enableIBCEventLogs);
    detailedLogger_.enableCategory(LogCategory::NetworkDrops, simCfg_.enableNetworkDropLogs);
    detailedLogger_.enableCategory(LogCategory::NodeState, simCfg_.enableNodeStateSnapshots);
    detailedLogger_.enableCategory(LogCategory::RelayerState, simCfg_.enableRelayerStateLogs);
}

Status SimulationController::init() {
    rootLog_.info("Initializing simulation...");

    // Create chains and nodes
    for (const auto& chainCfg : chainCfgs_) {
        auto chain = std::make_unique<Blockchain>(chainCfg.chainId, bus_, rootLog_, metrics_, &detailedLogger_);
        std::string chain_mailbox_address; // To store the address for the relayers
        for (size_t i = 0; i < chainCfg.nodeCount; ++i) {
            std::string nodeId = "node-" + std::to_string(i);
            std::string address = chain->id() + ":" + nodeId;
            if (i == 0) { // Use the first node's address as the chain's mailbox
                chain_mailbox_address = address;
            }
            auto consensus = ConsensusFactory::make(chainCfg, metrics_);
            nodes_.push_back(std::make_unique<Node>(nodeId, *chain, std::move(consensus), transport_, address, rootLog_, metrics_, &detailedLogger_));
        }
        chains_.push_back(std::move(chain));

        // Connect this chain's mailbox to all relayers
        if (!chain_mailbox_address.empty()) {
            // Store for later relayer connection
            for (size_t r = 0; r < simCfg_.relayerCount; ++r) {
                if (r >= relayers_.size()) {
                    // Create relayer if not yet created
                    std::string relayerId = "relayer-" + std::to_string(r);
                    relayers_.push_back(
                        std::make_unique<Relayer>(transport_, bus_, relayerId, rootLog_, metrics_, &detailedLogger_)
                    );
                }
                relayers_[r]->connectChainMailbox(chainCfg.chainId, chain_mailbox_address);
            }
        }
    }

    rootLog_.info("Simulation initialized with " + std::to_string(relayers_.size()) + " relayers.");
    return {ErrorCode::Ok, ""};
}

Status SimulationController::openIBC(const std::string& a, PortId ap, ChannelId ac,
                                     const std::string& b, PortId bp, ChannelId bc) {
    rootLog_.info("Opening IBC channel between " + a + " and " + b);
    Blockchain* chainA = findChain(a);
    Blockchain* chainB = findChain(b);
    if (!chainA || !chainB) {
        return {ErrorCode::NotFound, "One or both chains not found"};
    }
    chainA->openChannel(ap, ac);
    chainB->openChannel(bp, bc);
    return {ErrorCode::Ok, ""};
}

Status SimulationController::start() {
    rootLog_.info("Starting simulation nodes...");
    for (auto& node : nodes_) {
        auto status = node->start();
        if (!status.ok()) {
            return status;
        }
    }
    rootLog_.info("All nodes started.");

    // Start all relayers
    rootLog_.info("Starting " + std::to_string(relayers_.size()) + " relayers...");
    for (auto& relayer : relayers_) {
        auto relayerStatus = relayer->start();
        if (!relayerStatus.ok()) {
            rootLog_.error("Failed to start relayer " + relayer->getRelayerId() + ": " + relayerStatus.message);
            return relayerStatus;
        }
    }
    rootLog_.info("All relayers started.");

    // Start continuous traffic generator
    if (simCfg_.enableContinuousTraffic)
    {
        rootLog_.info("Starting traffic generator...");
        trafficRunning_ = true;
        trafficThread_ = std::thread([this]() { trafficGeneratorLoop(); });
        rootLog_.info("Traffic generator started.");
    }

    return {ErrorCode::Ok, ""};
}

void SimulationController::stop() {
    // Stop traffic generator first
    if (trafficRunning_.exchange(false))
    {
        rootLog_.info("Stopping traffic generator...");
        if (trafficThread_.joinable())
        {
            trafficThread_.join();
        }
        rootLog_.info("Traffic generator stopped.");
    }

    rootLog_.info("Stopping relayers...");
    for (auto& relayer : relayers_) {
        relayer->stop();
    }
    rootLog_.info("All relayers stopped.");

    rootLog_.info("Stopping simulation nodes...");
    for (auto& node : nodes_) {
        node->stop();
    }
    rootLog_.info("All nodes stopped.");

    // Flush all detailed logs
    rootLog_.info("Flushing detailed logs...");
    detailedLogger_.flushAll();
    rootLog_.info("All logs flushed.");
}

void SimulationController::injectTraffic() {
    rootLog_.info("Injecting traffic...");

    std::random_device rd;
    std::mt19937 rng(rd());

    // 1. Collect all node addresses
    std::vector<std::string> all_node_addresses;
    for (const auto& node_ptr : nodes_) {
        all_node_addresses.push_back(node_ptr->address());
    }

    // 2. Generate regular transactions
    if (!all_node_addresses.empty()) {
        std::uniform_int_distribution<size_t> node_idx_dist(0, all_node_addresses.size() - 1);
        for (const auto& sender_node_ptr : nodes_) {
            // Each node sends a few transactions
            for (int i = 0; i < 5; ++i) {
                std::string recipient_address = all_node_addresses[node_idx_dist(rng)];
                Transaction tx;
                tx.from = sender_node_ptr->address();
                tx.to = recipient_address;
                tx.payload = "regular_tx_from_" + sender_node_ptr->address() + "_to_" + recipient_address + "_seq_" + std::to_string(i);
                tx.type = TxType::Regular;
                tx.tx_id = generateTxId();

                // Log transaction creation
                if (simCfg_.enableDetailedTransactionLogs) {
                    detailedLogger_.logTransactionEvent(
                        TxEventType::Created,
                        tx.tx_id,
                        txTypeToString(tx.type),
                        tx.from,
                        tx.to,
                        tx.payload
                    );
                }

                sender_node_ptr->submitTransaction(tx);
            }
        }
    } else {
        rootLog_.warn("No nodes available to inject regular traffic.");
    }

    // 3. Generate IBC transactions
    if (chains_.size() >= 2) {
        std::uniform_int_distribution<size_t> chain_idx_dist(0, chains_.size() - 1);
        // Send a few IBC transactions
        for (int i = 0; i < 2; ++i) {
            size_t src_chain_idx = chain_idx_dist(rng);
            size_t dst_chain_idx = chain_idx_dist(rng);
            while (dst_chain_idx == src_chain_idx) { // Ensure different chains
                dst_chain_idx = chain_idx_dist(rng);
            }

            Blockchain* src_chain = chains_[src_chain_idx].get();
            Blockchain* dst_chain = chains_[dst_chain_idx].get();

            // Hardcoded port/channel IDs for simplicity, assuming they are opened
            // These channels need to be opened via controller.openIBC() in main.cpp
            PortId src_port{"port-A"};
            ChannelId src_chan{"channel-A"};
            PortId dst_port{"port-B"};
            ChannelId dst_chan{"channel-B"};

            Result<IBCPacket> pkt_res = src_chain->sendIBC(src_port, src_chan,
                                                           dst_chain->id(), dst_port, dst_chan,
                                                           "ibc_payload_from_" + src_chain->id() + "_to_" + dst_chain->id() + "_seq_" + std::to_string(i));
            if (!pkt_res.status.ok()) {
                rootLog_.warn("Failed to send IBC packet from " + src_chain->id() + ": " + pkt_res.status.message);
            } else {
                rootLog_.info("Sent IBC packet from " + src_chain->id() + " to " + dst_chain->id() + " (will be auto-relayed)");
                // Packet is now automatically relayed via EventBus
            }
        }
    } else {
        rootLog_.warn("Not enough chains to inject IBC traffic.");
    }

    rootLog_.info("Traffic injection complete.");
}

void SimulationController::run() {
    rootLog_.info("Running simulation for " + std::to_string(simCfg_.runFor.count()) + "ms");
    std::this_thread::sleep_for(simCfg_.runFor);
    rootLog_.info("Simulation run finished.");
}

Blockchain* SimulationController::findChain(const std::string& id) {
    auto it = std::find_if(chains_.begin(), chains_.end(),
                           [&](const std::unique_ptr<Blockchain>& chain) {
                               return chain->id() == id;
                           });
    if (it != chains_.end()) {
        return it->get();
    }
    return nullptr;
}

void SimulationController::trafficGeneratorLoop()
{
    rootLog_.info("Traffic generator loop started");

    std::exponential_distribution<double> interval_dist(
        1000.0 / simCfg_.trafficGenInterval.count()
    );
    std::uniform_real_distribution<double> type_dist(0.0, 1.0);

    while (trafficRunning_)
    {
        // Calculate next interval (Poisson process)
        auto wait_ms = static_cast<long>(interval_dist(trafficRng_));
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

        if (!trafficRunning_) break;

        // Decide transaction type
        double type_rand = type_dist(trafficRng_);

        if (type_rand < simCfg_.ibcTrafficRatio && chains_.size() >= 2)
        {
            // Generate IBC packet
            generateRandomIBCPacket();
        }
        else if (!nodes_.empty())
        {
            // Generate regular transaction
            generateRandomTransaction();
        }
    }

    rootLog_.info("Traffic generator loop finished");
}

void SimulationController::generateRandomTransaction()
{
    if (nodes_.empty()) return;

    std::uniform_int_distribution<size_t> node_dist(0, nodes_.size() - 1);

    size_t sender_idx = node_dist(trafficRng_);
    size_t receiver_idx = node_dist(trafficRng_);

    Node* sender = nodes_[sender_idx].get();
    Node* receiver = nodes_[receiver_idx].get();

    Transaction tx;
    tx.from = sender->address();
    tx.to = receiver->address();
    tx.payload = "auto_gen_tx_" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count()
    );
    tx.type = TxType::Regular;
    tx.tx_id = generateTxId();

    // Log transaction creation
    if (simCfg_.enableDetailedTransactionLogs) {
        detailedLogger_.logTransactionEvent(
            TxEventType::Created,
            tx.tx_id,
            txTypeToString(tx.type),
            tx.from,
            tx.to,
            tx.payload
        );
    }

    sender->submitTransaction(tx);
    metrics_.incCounter("traffic_regular_tx_generated");
}

void SimulationController::generateRandomIBCPacket()
{
    if (chains_.size() < 2) return;

    std::uniform_int_distribution<size_t> chain_dist(0, chains_.size() - 1);

    size_t src_idx = chain_dist(trafficRng_);
    size_t dst_idx = chain_dist(trafficRng_);

    // Ensure different chains
    while (dst_idx == src_idx && chains_.size() > 1)
    {
        dst_idx = chain_dist(trafficRng_);
    }

    Blockchain* src_chain = chains_[src_idx].get();
    Blockchain* dst_chain = chains_[dst_idx].get();

    // Use default port/channel (assumes they're opened in init)
    PortId src_port{"port-A"};
    ChannelId src_chan{"channel-A"};
    PortId dst_port{"port-B"};
    ChannelId dst_chan{"channel-B"};

    std::string payload = "auto_ibc_" + src_chain->id() + "_to_" +
                          dst_chain->id() + "_" +
                          std::to_string(
                              std::chrono::system_clock::now().time_since_epoch().count()
                          );

    Result<IBCPacket> pkt_res = src_chain->sendIBC(
        src_port, src_chan,
        dst_chain->id(), dst_port, dst_chan,
        payload
    );

    if (pkt_res.status.ok())
    {
        metrics_.incCounter("traffic_ibc_tx_generated");
    }
    else
    {
        rootLog_.warn("Failed to generate IBC packet: " + pkt_res.status.message);
        metrics_.incCounter("traffic_ibc_tx_failed");
    }
}
