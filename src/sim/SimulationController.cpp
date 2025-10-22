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
      netParams_{simCfg.defaultLinkLatency, simCfg.packetDropRate},
      transport_(simCfg.rngSeed, netParams_),
      relayer_(std::make_unique<Relayer>(transport_, bus_, "relayer", rootLog_, metrics_))
{
    for (const auto& chainCfg : chains) {
        chainCfgs_.push_back(chainCfg);
    }
}

Status SimulationController::init() {
    rootLog_.info("Initializing simulation...");

    for (const auto& chainCfg : chainCfgs_) {
        auto chain = std::make_unique<Blockchain>(chainCfg.chainId, bus_, rootLog_, metrics_);
        std::string chain_mailbox_address; // To store the address for the relayer
        for (size_t i = 0; i < chainCfg.nodeCount; ++i) {
            std::string nodeId = "node-" + std::to_string(i);
            std::string address = chain->id() + ":" + nodeId;
            if (i == 0) { // Use the first node's address as the chain's mailbox
                chain_mailbox_address = address;
            }
            auto consensus = ConsensusFactory::make(chainCfg, metrics_);
            nodes_.push_back(std::make_unique<Node>(nodeId, *chain, std::move(consensus), transport_, address, rootLog_, metrics_));
        }
        chains_.push_back(std::move(chain));
        // Connect this chain's mailbox to the relayer
        if (!chain_mailbox_address.empty()) {
            relayer_->connectChainMailbox(chainCfg.chainId, chain_mailbox_address);
        }
    }

    rootLog_.info("Simulation initialized.");
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

    // Start the relayer
    rootLog_.info("Starting relayer...");
    auto relayerStatus = relayer_->start();
    if (!relayerStatus.ok()) {
        rootLog_.error("Failed to start relayer: " + relayerStatus.message);
        return relayerStatus;
    }
    rootLog_.info("Relayer started.");

    return {ErrorCode::Ok, ""};
}

void SimulationController::stop() {
    rootLog_.info("Stopping relayer...");
    relayer_->stop();
    rootLog_.info("Relayer stopped.");

    rootLog_.info("Stopping simulation nodes...");
    for (auto& node : nodes_) {
        node->stop();
    }
    rootLog_.info("All nodes stopped.");
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
