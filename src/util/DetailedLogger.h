// util/DetailedLogger.h
// Manages multiple JSONL output streams for detailed event logging
#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <unordered_map>

// Forward declarations
struct Transaction;
struct IBCPacket;

// Detailed logging categories
enum class LogCategory
{
    Transactions,
    IBCEvents,
    NetworkDrops,
    NodeState,
    RelayerState
};

// Transaction event types
enum class TxEventType
{
    Created,
    Submitted,
    Received,
    IncludedInBlock,
    Dropped
};

// IBC event types
enum class IBCEventType
{
    PacketCreated,
    PacketRelayed,
    PacketReceived,
    AckGenerated,
    AckRelayed,
    AckReceived
};

// A single logger output stream
class LogStream
{
public:
    explicit LogStream(const std::string& filename);
    ~LogStream();

    void write(const std::string& json_line);
    void flush();

private:
    std::ofstream file_;
    std::mutex mutex_;
};

// Main detailed logging coordinator
class DetailedLogger
{
public:
    DetailedLogger();
    ~DetailedLogger();

    // Transaction lifecycle logging
    void logTransactionEvent(
        TxEventType event_type,
        const std::string& tx_id,
        const std::string& tx_type,
        const std::string& from,
        const std::string& to,
        const std::string& payload,
        const std::string& chain_id = "",
        const std::string& node_id = "",
        uint64_t block_height = 0
    );

    // IBC event logging
    void logIBCEvent(
        IBCEventType event_type,
        const std::string& src_chain,
        const std::string& dst_chain,
        const std::string& src_port,
        const std::string& src_channel,
        const std::string& dst_port,
        const std::string& dst_channel,
        uint64_t sequence,
        const std::string& payload,
        const std::string& relayer_id = "",
        double latency_ms = 0.0
    );

    // Network drop logging
    void logNetworkDrop(
        const std::string& from,
        const std::string& to,
        const std::string& message_type,
        size_t message_size,
        const std::string& drop_reason
    );

    // Node state snapshot
    void logNodeState(
        const std::string& chain_id,
        const std::string& node_id,
        uint64_t block_height,
        const std::string& block_hash,
        size_t mempool_size,
        const std::string& consensus_state,
        const std::string& additional_data = ""
    );

    // Relayer state logging
    void logRelayerState(
        const std::string& relayer_id,
        const std::string& event_type,
        uint64_t packets_relayed,
        uint64_t acks_relayed,
        uint64_t failures,
        const std::string& additional_data = ""
    );

    // Enable/disable categories
    void enableCategory(LogCategory category, bool enabled);

    // Flush all streams
    void flushAll();

private:
    std::unique_ptr<LogStream> transactions_log_;
    std::unique_ptr<LogStream> ibc_events_log_;
    std::unique_ptr<LogStream> network_drops_log_;

    // Per-node state logs (created on demand)
    std::unordered_map<std::string, std::unique_ptr<LogStream>> node_state_logs_;
    std::mutex node_logs_mutex_;

    // Per-relayer state logs (created on demand)
    std::unordered_map<std::string, std::unique_ptr<LogStream>> relayer_state_logs_;
    std::mutex relayer_logs_mutex_;

    // Category enable flags
    bool transactions_enabled_{true};
    bool ibc_events_enabled_{true};
    bool network_drops_enabled_{true};
    bool node_state_enabled_{true};
    bool relayer_state_enabled_{true};

    // Helper methods
    std::string nowIso8601() const;
    std::string escapeJson(const std::string& input) const;
    LogStream* getNodeStateLog(const std::string& chain_id, const std::string& node_id);
    LogStream* getRelayerStateLog(const std::string& relayer_id);
};
