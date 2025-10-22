// core/Types.h
// Basic hashes/ids.
#pragma once
#include <string>

using Hash = std::string;

struct PeerId
{
    std::string chainId;
    std::string nodeId;
};
