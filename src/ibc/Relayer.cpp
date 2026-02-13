// filepath: /home/niishaaant/work/blockchain-comm-sim/src/ibc/Relayer.cpp

#include "Relayer.h"
#include "core/Node.h"
#include "util/DetailedLogger.h"
#include <random>
#include <mutex>
#include <sstream>

namespace
{
    // Helper for random drop
    bool shouldDrop(std::mt19937 &rng, double dropRate)
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < dropRate;
    }
}

Relayer::Relayer(Transport &transport, EventBus &bus, const std::string &name, Logger &log, MetricsSink &metrics, DetailedLogger* detailedLogger)
    : transport_(transport), bus_(bus), name_(name), log_(log), metrics_(metrics), detailedLogger_(detailedLogger)
{
    // Seed RNG with hash of name for deterministic behavior per relayer
    std::hash<std::string> hasher;
    rng_ = std::mt19937(static_cast<unsigned>(hasher(name)));

    // Subscribe to IBC events
    packetSendToken_ = bus_.subscribe(EventKind::IBCPacketSend,
        [this](const Event &e) { this->onIBCPacketSendEvent(e); });
    ackSendToken_ = bus_.subscribe(EventKind::IBCAckSend,
        [this](const Event &e) { this->onIBCAckSendEvent(e); });

    log_.info("Relayer '" + name_ + "' initialized with event subscriptions");
}

Relayer::~Relayer()
{
    stop();
    if (packetSendToken_ != -1) {
        bus_.unsubscribe(packetSendToken_);
    }
    if (ackSendToken_ != -1) {
        bus_.unsubscribe(ackSendToken_);
    }
}

Status Relayer::connectChainMailbox(const std::string &chainId, const std::string &address)
{
    std::lock_guard<std::mutex> lock(mtx_);
    chainAddr_[chainId] = address;
    return {ErrorCode::Ok, ""};
}

Status Relayer::relayPacket(const IBCPacket &pkt)
{
    std::string dstChain = pkt.dstChain;
    std::string srcChain = pkt.srcChain;
    std::string payload = pkt.payload;

    std::string toAddr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = chainAddr_.find(dstChain);
        if (it == chainAddr_.end())
            return {ErrorCode::NotFound, "Destination chain not connected"};
        toAddr = it->second;
    }

    // Simulate route drop
    if (shouldDrop(rng_, routeDrop_))
        return {ErrorCode::NetworkDrop, "Packet dropped on relayer route"};

    // // Serialize IBCPacket as bytes (simple string for now)
    // Transport::Bytes bytes(payload.begin(), payload.end());

    // // Send via transport
    // return transport_.send(name_, toAddr, bytes);

    NodeMessage msg;
    msg.fromAddress = name_;
    msg.kind = NodeMessageKind::IBC;
    msg.bytes = serializeIBCPacket(pkt); // Send full serialized IBCPacket, not just payload
                         // Use the same on-wire encoding as Node::serializeNodeMessage
    auto serializeNodeMessage = [](const NodeMessage &m)
    {
        std::ostringstream oss;
        oss << m.fromAddress << "|" << static_cast<int>(m.kind) << "|" << m.bytes;
        return oss.str();
    };
    return transport_.send(name_, toAddr, serializeNodeMessage(msg));
}

Status Relayer::relayAck(const IBCPacket &ackPacket)
{
    std::string dstChain = ackPacket.dstChain;
    std::string srcChain = ackPacket.srcChain;
    std::string payload = ackPacket.payload;

    std::string toAddr;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = chainAddr_.find(dstChain);
        if (it == chainAddr_.end())
            return {ErrorCode::NotFound, "Destination chain not connected"};
        toAddr = it->second;
    }

    // // Simulate route drop
    // if (shouldDrop(rng_, routeDrop_))
    //     return {ErrorCode::NetworkDrop, "Ack dropped on relayer route"};

    // // Serialize IBCPacket as bytes (simple string for now)
    // Transport::Bytes bytes(payload.begin(), payload.end());

    // // Send via transport
    // return transport_.send(name_, toAddr, bytes);

    if (shouldDrop(rng_, routeDrop_))
        return {ErrorCode::NetworkDrop, "Ack dropped on relayer route"};
    NodeMessage msg;
    msg.fromAddress = name_;
    msg.kind = NodeMessageKind::IBC;
    msg.bytes = serializeIBCPacket(ackPacket); // Send full serialized IBCPacket (ack), not just payload
    auto serializeNodeMessage = [](const NodeMessage &m)
    {
        std::ostringstream oss;
        oss << m.fromAddress << "|" << static_cast<int>(m.kind) << "|" << m.bytes;
        return oss.str();
    };
    return transport_.send(name_, toAddr, serializeNodeMessage(msg));
}

void Relayer::setDropOnRoute(double probability)
{
    std::lock_guard<std::mutex> lock(mtx_);
    routeDrop_ = probability;
}

Status Relayer::start()
{
    if (running_.exchange(true))
    {
        return {ErrorCode::InvalidState, "Relayer already running"};
    }
    worker_ = std::thread([this]() { runLoop(); });
    log_.info("Relayer '" + name_ + "' started");
    return {ErrorCode::Ok, ""};
}

void Relayer::stop()
{
    if (!running_.exchange(false))
        return;

    pendingPackets_.close();
    pendingAcks_.close();

    if (worker_.joinable())
    {
        worker_.join();
    }
    log_.info("Relayer '" + name_ + "' stopped");
}

void Relayer::runLoop()
{
    log_.info("Relayer '" + name_ + "' run loop started");

    while (running_)
    {
        // Process pending packets with a timeout approach
        bool processed = false;

        // Try to get a packet (non-blocking)
        auto pktOpt = pendingPackets_.tryPop();
        if (pktOpt.has_value())
        {
            IBCPacket pkt = pktOpt.value();
            log_.info("Relaying packet from " + pkt.srcChain + " to " + pkt.dstChain + " (seq=" + std::to_string(pkt.sequence) + ")");

            Status s = relayPacket(pkt);
            if (s.ok())
            {
                packetsRelayed_++;
                metrics_.incCounter("relayer_packets_relayed");
                log_.debug("Successfully relayed packet seq=" + std::to_string(pkt.sequence));

                // Detailed IBC event logging
                if (detailedLogger_)
                {
                    detailedLogger_->logIBCEvent(
                        IBCEventType::PacketRelayed,
                        pkt.srcChain,
                        pkt.dstChain,
                        pkt.srcPort.value,
                        pkt.srcChannel.value,
                        pkt.dstPort.value,
                        pkt.dstChannel.value,
                        pkt.sequence,
                        pkt.payload,
                        name_
                    );
                }

                logRelayerState("packet_relayed", "seq=" + std::to_string(pkt.sequence));
            }
            else
            {
                failures_++;
                metrics_.incCounter("relayer_packets_failed");
                log_.warn("Failed to relay packet: " + s.message);
                logRelayerState("packet_failed", s.message);
            }
            processed = true;
        }

        // Try to get an ack (non-blocking)
        auto ackOpt = pendingAcks_.tryPop();
        if (ackOpt.has_value())
        {
            IBCPacket ack = ackOpt.value();
            log_.info("Relaying ack from " + ack.srcChain + " to " + ack.dstChain + " (seq=" + std::to_string(ack.sequence) + ")");

            Status s = relayAck(ack);
            if (s.ok())
            {
                acksRelayed_++;
                metrics_.incCounter("relayer_acks_relayed");
                log_.debug("Successfully relayed ack seq=" + std::to_string(ack.sequence));

                // Detailed IBC event logging
                if (detailedLogger_)
                {
                    detailedLogger_->logIBCEvent(
                        IBCEventType::AckRelayed,
                        ack.srcChain,
                        ack.dstChain,
                        ack.srcPort.value,
                        ack.srcChannel.value,
                        ack.dstPort.value,
                        ack.dstChannel.value,
                        ack.sequence,
                        ack.payload,
                        name_
                    );
                }

                logRelayerState("ack_relayed", "seq=" + std::to_string(ack.sequence));
            }
            else
            {
                failures_++;
                metrics_.incCounter("relayer_acks_failed");
                log_.warn("Failed to relay ack: " + s.message);
                logRelayerState("ack_failed", s.message);
            }
            processed = true;
        }

        // If nothing was processed, sleep briefly to avoid busy-waiting
        if (!processed)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    log_.info("Relayer '" + name_ + "' run loop finished");
}

void Relayer::onIBCPacketSendEvent(const Event &e)
{
    try {
        IBCPacket pkt = deserializeIBCPacket(e.detail);

        // Only relay Data packets (not Acks)
        if (pkt.type == IBCPacketType::Data) {
            pendingPackets_.push(pkt);
            log_.debug("Queued IBC packet from " + pkt.srcChain +
                       " to " + pkt.dstChain + " (seq=" +
                       std::to_string(pkt.sequence) + ")");
            metrics_.incCounter("relayer_packets_queued");
        }
    } catch (const std::exception& ex) {
        log_.error("Failed to deserialize IBCPacket: " +
                   std::string(ex.what()));
        metrics_.incCounter("relayer_deserialization_errors");
    }
}

void Relayer::onIBCAckSendEvent(const Event &e)
{
    try {
        IBCPacket ack = deserializeIBCPacket(e.detail);

        if (ack.type == IBCPacketType::Ack) {
            pendingAcks_.push(ack);
            log_.debug("Queued IBC ack from " + ack.srcChain +
                       " to " + ack.dstChain + " (seq=" +
                       std::to_string(ack.sequence) + ")");
            metrics_.incCounter("relayer_acks_queued");
        }
    } catch (const std::exception& ex) {
        log_.error("Failed to deserialize IBCAck: " +
                   std::string(ex.what()));
        metrics_.incCounter("relayer_deserialization_errors");
    }
}

void Relayer::logRelayerState(const std::string& event_type, const std::string& additional_data)
{
    if (detailedLogger_)
    {
        detailedLogger_->logRelayerState(
            name_,
            event_type,
            packetsRelayed_.load(),
            acksRelayed_.load(),
            failures_.load(),
            additional_data
        );
    }
}