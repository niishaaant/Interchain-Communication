#include "Node.h"
#include "util/DetailedLogger.h"
#include <stdexcept>
#include <sstream>

// Helper: serialize/deserialize NodeMessage (simple, not robust)
static std::string serializeNodeMessage(const NodeMessage &msg)
{
    // Format: fromAddress|kind|bytes
    std::ostringstream oss;
    oss << msg.fromAddress << "|" << static_cast<int>(msg.kind) << "|" << msg.bytes;
    return oss.str();
}

static NodeMessage deserializeNodeMessage(const std::string &s)
{
    NodeMessage msg;
    size_t p1 = s.find('|');
    size_t p2 = s.find('|', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos)
        throw std::runtime_error("Malformed NodeMessage");
    msg.fromAddress = s.substr(0, p1);
    msg.kind = static_cast<NodeMessageKind>(std::stoi(s.substr(p1 + 1, p2 - p1 - 1)));
    msg.bytes = s.substr(p2 + 1);
    return msg;
}

Node::Node(const std::string &nodeId,
           Blockchain &chainRef,
           std::unique_ptr<Consensus> consensus,
           Transport &transport,
           const std::string &address,
           Logger &log,
           MetricsSink &metrics,
           DetailedLogger* detailedLogger)
    : nodeId_(nodeId),
      chain_(chainRef),
      consensus_(std::move(consensus)),
      transport_(transport),
      address_(address),
      log_(log),
      metrics_(metrics),
      detailedLogger_(detailedLogger)
{
    // Register endpoint for this node's address
    auto status = transport_.registerEndpoint(address_, [this](const std::string &bytes)
                                              { this->onBytes(bytes); });
    if (!status.ok())
    {
        log_.error("Failed to register endpoint: " + status.message);
        throw std::runtime_error("Transport endpoint registration failed");
    }
    chain_.registerNodeId(nodeId_);
}

Status Node::start()
{
    if (running_.exchange(true))
    {
        return {ErrorCode::InvalidState, "Node already running"};
    }
    worker_ = std::thread([this]
                          { runLoop(); });
    log_.info("Node " + nodeId_ + " started at address " + address_);
    return {ErrorCode::Ok, ""};
}

void Node::stop()
{
    if (!running_.exchange(false))
        return;
    inbox_.close();
    if (worker_.joinable())
    {
        worker_.join();
    }
    log_.info("Node " + nodeId_ + " stopped.");
}

std::string Node::address() const
{
    return address_;
}

void Node::submitTransaction(const Transaction &tx)
{
    // Add to local mempool and broadcast to peers
    chain_.mempool().add(tx);

    // Log transaction submission
    if (detailedLogger_)
    {
        detailedLogger_->logTransactionEvent(
            TxEventType::Submitted,
            tx.tx_id,
            txTypeToString(tx.type),
            tx.from,
            tx.to,
            tx.payload,
            chain_.id(),
            nodeId_
        );
    }

    NodeMessage msg;
    msg.fromAddress = address_;
    msg.kind = NodeMessageKind::Transaction;
    // For simplicity, serialize tx as from|to|payload|type|tx_id
    msg.bytes = tx.from + "|" + tx.to + "|" + tx.payload + "|" + std::to_string(static_cast<int>(tx.type)) + "|" + tx.tx_id;

    // Broadcast to all peers (simulate: in real, would have peer list)
    // Here, just send to self for demo
    transport_.send(address_, address_, serializeNodeMessage(msg));
    metrics_.incCounter("tx_submitted");
}

void Node::onBytes(const std::string &bytes)
{
    try
    {
        NodeMessage msg = deserializeNodeMessage(bytes);
        inbox_.push(std::move(msg));
    }
    catch (const std::exception &e)
    {
        log_.error("Failed to deserialize NodeMessage: " + std::string(e.what()));
    }
}

void Node::runLoop()
{
    while (running_)
    {
        NodeMessage msg;
        try
        {
            msg = inbox_.waitPop();
        }
        catch (const std::exception &)
        {
            // Queue closed, exit
            break;
        }

        switch (msg.kind)
        {
        case NodeMessageKind::Transaction:
        {
            // Deserialize tx: from|to|payload|type|tx_id
            size_t p1 = msg.bytes.find('|');
            size_t p2 = msg.bytes.find('|', p1 + 1);
            size_t p3 = msg.bytes.find('|', p2 + 1);
            size_t p4 = msg.bytes.find('|', p3 + 1);
            if (p1 == std::string::npos || p2 == std::string::npos ||
                p3 == std::string::npos || p4 == std::string::npos)
            {
                log_.warn("Malformed tx message");
                break;
            }
            Transaction tx;
            tx.from = msg.bytes.substr(0, p1);
            tx.to = msg.bytes.substr(p1 + 1, p2 - p1 - 1);
            tx.payload = msg.bytes.substr(p2 + 1, p3 - p2 - 1);
            tx.type = static_cast<TxType>(std::stoi(msg.bytes.substr(p3 + 1, p4 - p3 - 1)));
            tx.tx_id = msg.bytes.substr(p4 + 1);

            chain_.mempool().add(tx);
            metrics_.incCounter("tx_received");
            log_.debug("Node " + nodeId_ + " received tx from " + tx.from);

            // Log transaction received
            if (detailedLogger_)
            {
                detailedLogger_->logTransactionEvent(
                    TxEventType::Received,
                    tx.tx_id,
                    txTypeToString(tx.type),
                    tx.from,
                    tx.to,
                    tx.payload,
                    chain_.id(),
                    nodeId_
                );
            }

            // Snapshot state after receiving transaction
            snapshotState();
            break;
        }
        case NodeMessageKind::Block:
        {
            // Deserialize block (not implemented, placeholder)
            log_.debug("Node " + nodeId_ + " received block (not implemented)");
            break;
        }
        case NodeMessageKind::IBC:
        {
            // Deserialize IBC packet (not implemented, placeholder)
            log_.debug("Node " + nodeId_ + " received IBC packet (not implemented)");
            break;
        }
        default:
            log_.warn("Node " + nodeId_ + " received unknown message kind");
            break;
        }

        // Consensus step (simplified)
        if (consensus_)
        {
            // In a real system, consensus would be event-driven or timer-driven
            // Here, just call a tick or step method if available
            // consensus_->tick(); // Uncomment if Consensus has tick()
        }
    }
}

void Node::snapshotState()
{
    if (!detailedLogger_) return;

    // Capture current state
    const Block& head = chain_.head();
    size_t mempoolSize = chain_.mempool().size();

    // Use simple hash as placeholder (in reality would compute actual hash)
    std::string blockHash = "hash_" + std::to_string(head.header.height);

    std::string consensusState = consensus_ ? consensus_->name() : "none";

    detailedLogger_->logNodeState(
        chain_.id(),
        nodeId_,
        head.header.height,
        blockHash,
        mempoolSize,
        consensusState
    );
}