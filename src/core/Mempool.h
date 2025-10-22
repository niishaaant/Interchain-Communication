// core/Mempool.h
// Simple mempool for pending transactions.
#pragma once
#include <vector>
#include <optional>
#include "Transaction.h"

class Mempool
{
public:
    void add(const Transaction &tx);
    std::vector<Transaction> drain(size_t maxTxs);
    size_t size() const;

private:
    std::vector<Transaction> buf_;
    bool verify(const Transaction &tx);
};
