// util/DetailedLogger.cpp
#include "DetailedLogger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Helper: escape JSON string (basic)
static std::string escapeJsonString(const std::string &input)
{
    std::ostringstream ss;
    for (char c : input)
    {
        switch (c)
        {
        case '\"':
            ss << "\\\"";
            break;
        case '\\':
            ss << "\\\\";
            break;
        case '\b':
            ss << "\\b";
            break;
        case '\f':
            ss << "\\f";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                ss << "\\u"
                   << std::hex << std::setw(4) << std::setfill('0')
                   << static_cast<int>(static_cast<unsigned char>(c))
                   << std::dec << std::setfill(' ');
            }
            else
            {
                ss << c;
            }
        }
    }
    return ss.str();
}

// Helper: ISO8601 timestamp with milliseconds (UTC)
static std::string nowIso8601Timestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setw(3) << std::setfill('0') << ms.count() << 'Z';
    return ss.str();
}

// LogStream implementation
LogStream::LogStream(const std::string& filename)
{
    file_.open(filename, std::ofstream::out | std::ofstream::app);
    if (!file_.is_open())
    {
        throw std::runtime_error("LogStream: failed to open file: " + filename);
    }
}

LogStream::~LogStream()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
    {
        file_.flush();
        file_.close();
    }
}

void LogStream::write(const std::string& json_line)
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_ << json_line << '\n';
}

void LogStream::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    file_.flush();
}

// DetailedLogger implementation
DetailedLogger::DetailedLogger()
{
    transactions_log_ = std::make_unique<LogStream>("transactions.jsonl");
    ibc_events_log_ = std::make_unique<LogStream>("ibc_events.jsonl");
    network_drops_log_ = std::make_unique<LogStream>("network_drops.jsonl");
}

DetailedLogger::~DetailedLogger()
{
    flushAll();
}

std::string DetailedLogger::nowIso8601() const
{
    return nowIso8601Timestamp();
}

std::string DetailedLogger::escapeJson(const std::string& input) const
{
    return escapeJsonString(input);
}

void DetailedLogger::logTransactionEvent(
    TxEventType event_type,
    const std::string& tx_id,
    const std::string& tx_type,
    const std::string& from,
    const std::string& to,
    const std::string& payload,
    const std::string& chain_id,
    const std::string& node_id,
    uint64_t block_height)
{
    if (!transactions_enabled_) return;

    std::string event_name;
    switch (event_type)
    {
    case TxEventType::Created:
        event_name = "created";
        break;
    case TxEventType::Submitted:
        event_name = "submitted";
        break;
    case TxEventType::Received:
        event_name = "received";
        break;
    case TxEventType::IncludedInBlock:
        event_name = "included_in_block";
        break;
    case TxEventType::Dropped:
        event_name = "dropped";
        break;
    default:
        event_name = "unknown";
        break;
    }

    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJson(nowIso8601()) << "\",";
    ss << "\"event\":\"" << event_name << "\",";
    ss << "\"tx_id\":\"" << escapeJson(tx_id) << "\",";
    ss << "\"tx_type\":\"" << escapeJson(tx_type) << "\",";
    ss << "\"from\":\"" << escapeJson(from) << "\",";
    ss << "\"to\":\"" << escapeJson(to) << "\",";
    ss << "\"payload\":\"" << escapeJson(payload) << "\"";

    if (!chain_id.empty())
    {
        ss << ",\"chain_id\":\"" << escapeJson(chain_id) << "\"";
    }
    if (!node_id.empty())
    {
        ss << ",\"node_id\":\"" << escapeJson(node_id) << "\"";
    }
    if (block_height > 0)
    {
        ss << ",\"block_height\":" << block_height;
    }

    ss << "}";

    transactions_log_->write(ss.str());
}

void DetailedLogger::logIBCEvent(
    IBCEventType event_type,
    const std::string& src_chain,
    const std::string& dst_chain,
    const std::string& src_port,
    const std::string& src_channel,
    const std::string& dst_port,
    const std::string& dst_channel,
    uint64_t sequence,
    const std::string& payload,
    const std::string& relayer_id,
    double latency_ms)
{
    if (!ibc_events_enabled_) return;

    std::string event_name;
    switch (event_type)
    {
    case IBCEventType::PacketCreated:
        event_name = "packet_created";
        break;
    case IBCEventType::PacketRelayed:
        event_name = "packet_relayed";
        break;
    case IBCEventType::PacketReceived:
        event_name = "packet_received";
        break;
    case IBCEventType::AckGenerated:
        event_name = "ack_generated";
        break;
    case IBCEventType::AckRelayed:
        event_name = "ack_relayed";
        break;
    case IBCEventType::AckReceived:
        event_name = "ack_received";
        break;
    default:
        event_name = "unknown";
        break;
    }

    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJson(nowIso8601()) << "\",";
    ss << "\"event\":\"" << event_name << "\",";
    ss << "\"src_chain\":\"" << escapeJson(src_chain) << "\",";
    ss << "\"dst_chain\":\"" << escapeJson(dst_chain) << "\",";
    ss << "\"src_port\":\"" << escapeJson(src_port) << "\",";
    ss << "\"src_channel\":\"" << escapeJson(src_channel) << "\",";
    ss << "\"dst_port\":\"" << escapeJson(dst_port) << "\",";
    ss << "\"dst_channel\":\"" << escapeJson(dst_channel) << "\",";
    ss << "\"sequence\":" << sequence << ",";
    ss << "\"payload\":\"" << escapeJson(payload) << "\"";

    if (!relayer_id.empty())
    {
        ss << ",\"relayer_id\":\"" << escapeJson(relayer_id) << "\"";
    }
    if (latency_ms > 0.0)
    {
        ss << ",\"latency_ms\":" << latency_ms;
    }

    ss << "}";

    ibc_events_log_->write(ss.str());
}

void DetailedLogger::logNetworkDrop(
    const std::string& from,
    const std::string& to,
    const std::string& message_type,
    size_t message_size,
    const std::string& drop_reason)
{
    if (!network_drops_enabled_) return;

    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJson(nowIso8601()) << "\",";
    ss << "\"from\":\"" << escapeJson(from) << "\",";
    ss << "\"to\":\"" << escapeJson(to) << "\",";
    ss << "\"message_type\":\"" << escapeJson(message_type) << "\",";
    ss << "\"message_size\":" << message_size << ",";
    ss << "\"drop_reason\":\"" << escapeJson(drop_reason) << "\"";
    ss << "}";

    network_drops_log_->write(ss.str());
}

void DetailedLogger::logNodeState(
    const std::string& chain_id,
    const std::string& node_id,
    uint64_t block_height,
    const std::string& block_hash,
    size_t mempool_size,
    const std::string& consensus_state,
    const std::string& additional_data)
{
    if (!node_state_enabled_) return;

    LogStream* log = getNodeStateLog(chain_id, node_id);
    if (!log) return;

    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJson(nowIso8601()) << "\",";
    ss << "\"chain_id\":\"" << escapeJson(chain_id) << "\",";
    ss << "\"node_id\":\"" << escapeJson(node_id) << "\",";
    ss << "\"block_height\":" << block_height << ",";
    ss << "\"block_hash\":\"" << escapeJson(block_hash) << "\",";
    ss << "\"mempool_size\":" << mempool_size << ",";
    ss << "\"consensus_state\":\"" << escapeJson(consensus_state) << "\"";

    if (!additional_data.empty())
    {
        ss << ",\"additional\":\"" << escapeJson(additional_data) << "\"";
    }

    ss << "}";

    log->write(ss.str());
}

void DetailedLogger::logRelayerState(
    const std::string& relayer_id,
    const std::string& event_type,
    uint64_t packets_relayed,
    uint64_t acks_relayed,
    uint64_t failures,
    const std::string& additional_data)
{
    if (!relayer_state_enabled_) return;

    LogStream* log = getRelayerStateLog(relayer_id);
    if (!log) return;

    std::ostringstream ss;
    ss << "{";
    ss << "\"ts\":\"" << escapeJson(nowIso8601()) << "\",";
    ss << "\"relayer_id\":\"" << escapeJson(relayer_id) << "\",";
    ss << "\"event_type\":\"" << escapeJson(event_type) << "\",";
    ss << "\"packets_relayed\":" << packets_relayed << ",";
    ss << "\"acks_relayed\":" << acks_relayed << ",";
    ss << "\"failures\":" << failures;

    if (!additional_data.empty())
    {
        ss << ",\"additional\":\"" << escapeJson(additional_data) << "\"";
    }

    ss << "}";

    log->write(ss.str());
}

void DetailedLogger::enableCategory(LogCategory category, bool enabled)
{
    switch (category)
    {
    case LogCategory::Transactions:
        transactions_enabled_ = enabled;
        break;
    case LogCategory::IBCEvents:
        ibc_events_enabled_ = enabled;
        break;
    case LogCategory::NetworkDrops:
        network_drops_enabled_ = enabled;
        break;
    case LogCategory::NodeState:
        node_state_enabled_ = enabled;
        break;
    case LogCategory::RelayerState:
        relayer_state_enabled_ = enabled;
        break;
    }
}

void DetailedLogger::flushAll()
{
    if (transactions_log_) transactions_log_->flush();
    if (ibc_events_log_) ibc_events_log_->flush();
    if (network_drops_log_) network_drops_log_->flush();

    {
        std::lock_guard<std::mutex> lock(node_logs_mutex_);
        for (auto& pair : node_state_logs_)
        {
            pair.second->flush();
        }
    }

    {
        std::lock_guard<std::mutex> lock(relayer_logs_mutex_);
        for (auto& pair : relayer_state_logs_)
        {
            pair.second->flush();
        }
    }
}

LogStream* DetailedLogger::getNodeStateLog(const std::string& chain_id, const std::string& node_id)
{
    std::string key = chain_id + "_" + node_id;

    std::lock_guard<std::mutex> lock(node_logs_mutex_);

    auto it = node_state_logs_.find(key);
    if (it != node_state_logs_.end())
    {
        return it->second.get();
    }

    // Create new log stream
    std::string filename = "node_state_" + chain_id + "_" + node_id + ".jsonl";
    auto log = std::make_unique<LogStream>(filename);
    LogStream* ptr = log.get();
    node_state_logs_[key] = std::move(log);

    return ptr;
}

LogStream* DetailedLogger::getRelayerStateLog(const std::string& relayer_id)
{
    std::lock_guard<std::mutex> lock(relayer_logs_mutex_);

    auto it = relayer_state_logs_.find(relayer_id);
    if (it != relayer_state_logs_.end())
    {
        return it->second.get();
    }

    // Create new log stream
    std::string filename = "relayer_state_" + relayer_id + ".jsonl";
    auto log = std::make_unique<LogStream>(filename);
    LogStream* ptr = log.get();
    relayer_state_logs_[relayer_id] = std::move(log);

    return ptr;
}
