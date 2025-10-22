// ibc/IBCTypes.cpp
// Serialization utilities for IBC packets
#include "IBCTypes.h"
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
    // Helper to escape pipe characters in strings
    std::string escape(const std::string& str) {
        std::string result;
        for (char c : str) {
            if (c == '|') {
                result += "\\|";
            } else if (c == '\\') {
                result += "\\\\";
            } else {
                result += c;
            }
        }
        return result;
    }

    // Helper to unescape pipe characters
    std::string unescape(const std::string& str) {
        std::string result;
        bool escaped = false;
        for (char c : str) {
            if (escaped) {
                result += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else {
                result += c;
            }
        }
        return result;
    }

    // Split string by delimiter (not escaped)
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> result;
        std::string current;
        bool escaped = false;

        for (char c : str) {
            if (escaped) {
                current += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
                current += c;
            } else if (c == delimiter) {
                result.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        result.push_back(current);
        return result;
    }
}

std::string serializeIBCPacket(const IBCPacket& pkt) {
    std::ostringstream oss;

    // Format: type|srcChain|dstChain|srcPort|srcChan|dstPort|dstChan|seq|payload
    oss << static_cast<int>(pkt.type) << "|"
        << escape(pkt.srcChain) << "|"
        << escape(pkt.dstChain) << "|"
        << escape(pkt.srcPort.value) << "|"
        << escape(pkt.srcChannel.value) << "|"
        << escape(pkt.dstPort.value) << "|"
        << escape(pkt.dstChannel.value) << "|"
        << pkt.sequence << "|"
        << escape(pkt.payload);

    return oss.str();
}

IBCPacket deserializeIBCPacket(const std::string& str) {
    std::vector<std::string> parts = split(str, '|');

    if (parts.size() != 9) {
        throw std::runtime_error("Invalid IBCPacket serialization format: expected 9 parts, got " +
                                 std::to_string(parts.size()));
    }

    IBCPacket pkt;

    try {
        // Parse type
        int typeInt = std::stoi(parts[0]);
        pkt.type = static_cast<IBCPacketType>(typeInt);

        // Parse strings
        pkt.srcChain = unescape(parts[1]);
        pkt.dstChain = unescape(parts[2]);
        pkt.srcPort.value = unescape(parts[3]);
        pkt.srcChannel.value = unescape(parts[4]);
        pkt.dstPort.value = unescape(parts[5]);
        pkt.dstChannel.value = unescape(parts[6]);

        // Parse sequence
        pkt.sequence = std::stoull(parts[7]);

        // Parse payload
        pkt.payload = unescape(parts[8]);

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse IBCPacket: " + std::string(e.what()));
    }

    return pkt;
}
