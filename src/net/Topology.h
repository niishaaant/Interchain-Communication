// net/Topology.h
// Describes how nodes connect within/between chains (logical overlay).
#pragma once
#include <vector>
#include <string>
#include "core/Types.h"

struct LinkSpec
{
    PeerId from;
    PeerId to;
};

class TopologyImpl;

class Topology
{
public:
    Topology();
    ~Topology();

    void addLink(const LinkSpec &link);
    std::vector<PeerId> neighbors(const PeerId &p) const;

private:
    TopologyImpl *impl_;
};
