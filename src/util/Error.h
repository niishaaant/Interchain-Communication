// util/Error.h
// Unified error/status model for API surfaces.
#pragma once
#include <string>
#include <optional>

enum class ErrorCode
{
    Ok,
    Timeout,
    NetworkDrop,
    InvalidState,
    Serialization,
    ConsensusFault,
    ChannelClosed,
    NotFound,
    Cancelled,
    Unknown
};

struct Status
{
    ErrorCode code{ErrorCode::Ok};
    std::string message{};
    bool ok() const { return code == ErrorCode::Ok; }
};

template <typename T>
struct Result
{
    Status status{};
    std::optional<T> value{};
};
