// core/Transaction.h
// Minimal TX used for intra/ inter-chain references.
#pragma once
#include <string>
#include <sstream>
#include <chrono>
#include <atomic>
#include "Types.h"

enum class TxType
{
    Regular,
    IBCPacket,
    IBCAck,
    Unknown
};

inline std::string txTypeToString(TxType type)
{
    switch (type)
    {
    case TxType::Regular:
        return "regular";
    case TxType::IBCPacket:
        return "ibc_packet";
    case TxType::IBCAck:
        return "ibc_ack";
    case TxType::Unknown:
    default:
        return "unknown";
    }
}

struct Transaction
{
    std::string from;
    std::string to;
    std::string payload; // opaque app/IBC payload
    TxType type{TxType::Unknown};
    std::string tx_id; // unique identifier
};

// Helper: generate unique transaction ID
inline std::string generateTxId()
{
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    uint64_t id = counter.fetch_add(1);
    std::ostringstream oss;
    oss << "tx_" << now << "_" << id;
    return oss.str();
}
