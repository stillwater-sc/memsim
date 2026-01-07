#pragma once

#include <cstdint>
#include <string_view>
#include <functional>
#include <optional>

namespace sw::memsim {

// ============================================================================
// Simulation Fidelity
// ============================================================================

/// Simulation fidelity levels
enum class Fidelity : uint8_t {
    BEHAVIORAL,      ///< Instant/fixed latency (~100-1000x faster)
    TRANSACTIONAL,   ///< Queue-based statistical timing (~10-100x faster)
    CYCLE_ACCURATE   ///< Full protocol state machines (1x baseline)
};

constexpr std::string_view to_string(Fidelity f) {
    switch (f) {
        case Fidelity::BEHAVIORAL:    return "BEHAVIORAL";
        case Fidelity::TRANSACTIONAL: return "TRANSACTIONAL";
        case Fidelity::CYCLE_ACCURATE: return "CYCLE_ACCURATE";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Memory Technologies
// ============================================================================

/// Supported memory technologies
enum class Technology : uint8_t {
    IDEAL,      ///< Configurable ideal memory
    DDR5,       ///< JEDEC DDR5
    LPDDR5,     ///< JEDEC LPDDR5
    LPDDR5X,    ///< JEDEC LPDDR5X
    LPDDR6,     ///< JEDEC LPDDR6 (future)
    HBM3,       ///< JEDEC HBM3
    HBM3E,      ///< JEDEC HBM3E
    HBM4,       ///< JEDEC HBM4 (future)
    GDDR6,      ///< JEDEC GDDR6
    GDDR7       ///< JEDEC GDDR7
};

constexpr std::string_view to_string(Technology t) {
    switch (t) {
        case Technology::IDEAL:   return "IDEAL";
        case Technology::DDR5:    return "DDR5";
        case Technology::LPDDR5:  return "LPDDR5";
        case Technology::LPDDR5X: return "LPDDR5X";
        case Technology::LPDDR6:  return "LPDDR6";
        case Technology::HBM3:    return "HBM3";
        case Technology::HBM3E:   return "HBM3E";
        case Technology::HBM4:    return "HBM4";
        case Technology::GDDR6:   return "GDDR6";
        case Technology::GDDR7:   return "GDDR7";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Request Types
// ============================================================================

/// Memory request type
enum class RequestType : uint8_t {
    READ,
    WRITE
};

constexpr std::string_view to_string(RequestType t) {
    switch (t) {
        case RequestType::READ:  return "READ";
        case RequestType::WRITE: return "WRITE";
        default: return "UNKNOWN";
    }
}

/// Request priority for QoS-aware scheduling
enum class Priority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    REALTIME = 3
};

// ============================================================================
// Bank State
// ============================================================================

/// DRAM bank state (for cycle-accurate simulation)
enum class BankState : uint8_t {
    IDLE,           ///< Precharged, no row open
    ACTIVATING,     ///< Row being opened (ACT issued)
    ACTIVE,         ///< Row open, ready for R/W
    READING,        ///< Read burst in progress
    WRITING,        ///< Write burst in progress
    PRECHARGING,    ///< Row being closed (PRE issued)
    REFRESHING      ///< Refresh in progress
};

constexpr std::string_view to_string(BankState s) {
    switch (s) {
        case BankState::IDLE:        return "IDLE";
        case BankState::ACTIVATING:  return "ACTIVATING";
        case BankState::ACTIVE:      return "ACTIVE";
        case BankState::READING:     return "READING";
        case BankState::WRITING:     return "WRITING";
        case BankState::PRECHARGING: return "PRECHARGING";
        case BankState::REFRESHING:  return "REFRESHING";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// Type Aliases
// ============================================================================

using Cycle = uint64_t;
using Address = uint64_t;
using Row = uint32_t;
using Column = uint16_t;
using Bank = uint8_t;
using BankGroup = uint8_t;
using Channel = uint8_t;
using Rank = uint8_t;
using RequestId = uint64_t;

/// Callback invoked when a request completes
/// Parameter is the latency in cycles
using CompletionCallback = std::function<void(Cycle latency)>;

// ============================================================================
// Memory Request
// ============================================================================

/// Memory request structure
struct Request {
    RequestId id = 0;           ///< Unique request identifier
    Address address = 0;        ///< Physical memory address
    uint32_t size = 0;          ///< Transfer size in bytes
    RequestType type = RequestType::READ;
    Priority priority = Priority::NORMAL;
    Cycle submit_cycle = 0;     ///< Cycle when request was submitted
    CompletionCallback callback = nullptr;

    // Decoded address components (filled by controller)
    Channel channel = 0;
    Rank rank = 0;
    BankGroup bank_group = 0;
    Bank bank = 0;
    Row row = 0;
    Column column = 0;
};

// ============================================================================
// Address Mapping
// ============================================================================

/// Address mapping scheme
enum class AddressMapping : uint8_t {
    ROW_BANK_COLUMN,     ///< Ro:Ba:Co - good for sequential access
    ROW_COLUMN_BANK,     ///< Ro:Co:Ba - good for strided access
    BANK_ROW_COLUMN,     ///< Ba:Ro:Co - bank interleaving
    CUSTOM               ///< User-defined bit mapping
};

} // namespace sw::memsim
