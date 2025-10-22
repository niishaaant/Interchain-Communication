#include "Mempool.h"

void Mempool::add(const Transaction &tx)
{
    if (verify(tx))
    {
        buf_.push_back(tx);
    }
}

std::vector<Transaction> Mempool::drain(size_t maxTxs)
{
    size_t n = std::min(maxTxs, buf_.size());
    std::vector<Transaction> drained(buf_.begin(), buf_.begin() + n);
    buf_.erase(buf_.begin(), buf_.begin() + n);
    return drained;
}

size_t Mempool::size() const
{
    return buf_.size();
}

bool Mempool::verify(const Transaction &tx)
{
    return true;
}