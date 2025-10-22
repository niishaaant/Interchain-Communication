// core/Transaction.h
// Minimal TX used for intra/ inter-chain references.
#pragma once
#include <string>
#include "Types.h"

struct Transaction
{
    std::string from;
    std::string to;
    std::string payload; // opaque app/IBC payload
};
