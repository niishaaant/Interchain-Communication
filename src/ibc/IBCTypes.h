// ibc/IBCTypes.h
// IBC-like packet, acknowledgements, ports/channels.
#pragma once
#include <string>
#include <cstdint>

enum class IBCPacketType
{
    Data,
    Ack
};

struct PortId
{
    std::string value;
};
struct ChannelId
{
    std::string value;
};

struct IBCPacket
{
    IBCPacketType type{IBCPacketType::Data};
    std::string srcChain;
    std::string dstChain;
    PortId srcPort;
    ChannelId srcChannel;
    PortId dstPort;
    ChannelId dstChannel;
    uint64_t sequence{0};
    std::string payload; // opaque app bytes
};

// Serialization utilities
std::string serializeIBCPacket(const IBCPacket& pkt);
IBCPacket deserializeIBCPacket(const std::string& str);
