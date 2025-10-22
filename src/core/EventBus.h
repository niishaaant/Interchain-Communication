#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

enum class EventKind
{
    BlockProposed,
    BlockFinalized,
    IBCPacketSend,
    IBCPacketRecv,
    IBCAckSend,
    IBCAckRecv,
    ConsensusRound,
    NetworkDrop,
    Error
};

struct Event
{
    EventKind kind;
    std::string chainId;
    std::string nodeId;
    std::string detail; // human-readable payload
};

class EventBus
{
public:
    using Handler = std::function<void(const Event &)>;
    int subscribe(EventKind kind, Handler h);
    void unsubscribe(int token);
    void publish(const Event &e);

private:
    int nextToken_{1};
    std::unordered_map<EventKind, std::vector<std::pair<int, Handler>>> subs_;
};
