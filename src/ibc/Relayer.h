// ibc/Relayer.h
// Off-chain relayer moving packets between chain mailboxes.
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include "IBCTypes.h"
#include "util/Error.h"
#include "net/Transport.h"
#include "core/EventBus.h"
#include "util/ConcurrentQueue.h"
#include "util/Logger.h"
#include "util/Metrics.h"

class Relayer
{
public:
    Relayer(Transport &transport, EventBus &bus, const std::string &name, Logger &log, MetricsSink &metrics);
    ~Relayer();

    Status connectChainMailbox(const std::string &chainId, const std::string &address);
    Status relayPacket(const IBCPacket &pkt);    // data
    Status relayAck(const IBCPacket &ackPacket); // ack
    void setDropOnRoute(double probability);     // additional route drop

    // Thread lifecycle
    Status start();
    void stop();

private:
    void runLoop(); // Main relayer thread loop
    void onIBCPacketSendEvent(const Event &e);
    void onIBCAckSendEvent(const Event &e);

    Transport &transport_;
    EventBus &bus_;
    std::string name_;
    Logger &log_;
    MetricsSink &metrics_;

    std::unordered_map<std::string, std::string> chainAddr_;
    std::mt19937 rng_;
    std::mutex mtx_;
    double routeDrop_{0.0};

    // Threading infrastructure
    std::thread worker_;
    std::atomic<bool> running_{false};
    ConcurrentQueue<IBCPacket> pendingPackets_;
    ConcurrentQueue<IBCPacket> pendingAcks_;

    // Event subscriptions
    int packetSendToken_{-1};
    int ackSendToken_{-1};
};
