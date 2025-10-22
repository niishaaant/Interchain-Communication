#include "Topology.h"
#include <mutex>
#include <algorithm>

// Helper function to compare PeerId
static bool peerIdEqual(const PeerId &a, const PeerId &b)
{
    return a.chainId == b.chainId && a.nodeId == b.nodeId;
}

class TopologyImpl
{
public:
    void addLink(const LinkSpec &link)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        links_.push_back(link);
    }

    std::vector<PeerId> neighbors(const PeerId &p) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<PeerId> result;
        for (const auto &link : links_)
        {
            if (peerIdEqual(link.from, p))
            {
                result.push_back(link.to);
            }
            // If undirected, also check reverse
            // if (peerIdEqual(link.to, p)) {
            //     result.push_back(link.from);
            // }
        }
        return result;
    }

private:
    mutable std::mutex mutex_;
    std::vector<LinkSpec> links_;
};

Topology::Topology() : impl_(new TopologyImpl) {}
Topology::~Topology() = default;

void Topology::addLink(const LinkSpec &link)
{
    impl_->addLink(link);
}

std::vector<PeerId> Topology::neighbors(const PeerId &p) const
{
    return impl_->neighbors(p);
}