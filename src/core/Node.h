// core/Node.h
// Networked node running on its own thread, driving consensus.
#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include "Transaction.h"
#include "Block.h"
#include "Blockchain.h"
#include "consensus/Consensus.h"
#include "net/Transport.h"
#include "util/ConcurrentQueue.h"
#include "util/Logger.h"
#include "util/Metrics.h"

// Forward declaration
class DetailedLogger;

enum class NodeMessageKind
{
    Block,
    Transaction,
    IBC,
    Unknown
};

inline std::string toString(NodeMessageKind kind)
{
    switch (kind)
    {
    case NodeMessageKind::Block:
        return "block";
    case NodeMessageKind::Transaction:
        return "tx";
    case NodeMessageKind::IBC:
        return "ibc";
    default:
        return "unknown";
    }
}

struct NodeMessage
{
    std::string fromAddress;
    NodeMessageKind kind; // "block","tx","ibc"
    std::string bytes;    // serialized payload
};

class Node
{
public:
    Node(const std::string &nodeId,
         Blockchain &chainRef,
         std::unique_ptr<Consensus> consensus,
         Transport &transport,
         const std::string &address,
         Logger &log,
         MetricsSink &metrics,
         DetailedLogger* detailedLogger = nullptr);
    ~Node();

    Status start(); // spawns thread
    void stop();    // signals and joins
    std::string address() const;

    // Local submissions
    void submitTransaction(const Transaction &tx);

    // Transport entry (registered DeliverFn calls this)
    void onBytes(const std::string &bytes);

private:
    void runLoop(); // thread main
    void snapshotState(); // captures current node state

    std::string nodeId_;
    Blockchain &chain_;
    std::unique_ptr<Consensus> consensus_;
    Transport &transport_;
    std::string address_;
    Logger &log_;
    MetricsSink &metrics_;
    DetailedLogger* detailedLogger_;
    std::thread worker_; // remember to join, they were jthreads
    std::atomic<bool> running_{false};
    ConcurrentQueue<NodeMessage> inbox_;
};
