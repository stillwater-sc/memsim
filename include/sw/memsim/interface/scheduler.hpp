#pragma once

#include <sw/memsim/core/types.hpp>

#include <memory>
#include <span>
#include <vector>

namespace sw::memsim {

// Forward declaration
class BankMachine;

/// Scheduler policy types
enum class SchedulerPolicy : uint8_t {
    FIFO,           ///< Simple FIFO per bank
    FR_FCFS,        ///< First-Ready FCFS (row hit priority)
    FR_FCFS_GRP,    ///< FR-FCFS with R/W grouping
    GRP_FR_FCFS,    ///< Grouping priority over row hits
    GRP_FR_FCFS_WM, ///< Grouping with watermark thresholds
    QOS_AWARE       ///< QoS-aware for mixed criticality
};

constexpr std::string_view to_string(SchedulerPolicy p) {
    switch (p) {
        case SchedulerPolicy::FIFO:          return "FIFO";
        case SchedulerPolicy::FR_FCFS:       return "FR_FCFS";
        case SchedulerPolicy::FR_FCFS_GRP:   return "FR_FCFS_GRP";
        case SchedulerPolicy::GRP_FR_FCFS:   return "GRP_FR_FCFS";
        case SchedulerPolicy::GRP_FR_FCFS_WM: return "GRP_FR_FCFS_WM";
        case SchedulerPolicy::QOS_AWARE:     return "QOS_AWARE";
        default: return "UNKNOWN";
    }
}

/// Buffer organization types
enum class BufferType : uint8_t {
    SHARED,         ///< Single shared buffer for all banks
    BANKWISE,       ///< Separate buffer per bank
    READ_WRITE      ///< Separate read and write buffers
};

/// Scheduler configuration
struct SchedulerConfig {
    SchedulerPolicy policy = SchedulerPolicy::FR_FCFS;
    BufferType buffer_type = BufferType::BANKWISE;

    uint32_t buffer_size = 32;           ///< Total buffer size
    uint32_t read_buffer_size = 16;      ///< For READ_WRITE buffer type
    uint32_t write_buffer_size = 16;     ///< For READ_WRITE buffer type

    // For watermark-based scheduling
    uint32_t high_watermark = 8;         ///< Switch to write when reads below
    uint32_t low_watermark = 4;          ///< Switch to read when writes below

    uint8_t num_banks = 16;              ///< Number of banks
};

/// Abstract scheduler interface
///
/// The scheduler manages the request buffer and determines which request
/// to issue next to each bank. Different policies optimize for different
/// workload characteristics:
///
/// - FIFO: Fair, simple, low hardware cost
/// - FR_FCFS: Maximizes row buffer hits
/// - FR_FCFS_GRP: Reduces read/write turnaround overhead
/// - GRP_FR_FCFS: Prioritizes grouping over row hits
/// - QOS_AWARE: Supports mixed-criticality workloads
class IScheduler {
public:
    virtual ~IScheduler() = default;

    // ========================================================================
    // Buffer Management
    // ========================================================================

    /// Check if buffer has space for N requests
    [[nodiscard]] virtual bool has_space(unsigned count = 1) const = 0;

    /// Store a request in the scheduler buffer
    virtual void store(Request& request) = 0;

    /// Remove a completed request from the buffer
    virtual void remove(const Request& request) = 0;

    /// Get current buffer occupancy
    [[nodiscard]] virtual size_t occupancy() const = 0;

    /// Get buffer depth per bank
    [[nodiscard]] virtual std::span<const unsigned> buffer_depth() const = 0;

    // ========================================================================
    // Request Selection
    // ========================================================================

    /// Get next request to issue for a bank
    ///
    /// The scheduler considers:
    /// - Row buffer state (for row hit prioritization)
    /// - Previous command type (for R/W grouping)
    /// - Request age (for fairness)
    /// - Request priority (for QoS)
    ///
    /// @param bank The bank to select a request for
    /// @param open_row Currently open row (nullopt if bank is precharged)
    /// @param last_cmd Last command type issued (for grouping)
    /// @return Pointer to selected request, or nullptr if none available
    [[nodiscard]] virtual Request* get_next(
        Bank bank,
        std::optional<Row> open_row,
        RequestType last_cmd) const = 0;

    /// Check if there's another row hit pending for this bank/row
    [[nodiscard]] virtual bool has_row_hit(
        Bank bank,
        Row row,
        RequestType type) const = 0;

    /// Check if there are more requests pending for this bank
    [[nodiscard]] virtual bool has_pending(
        Bank bank,
        RequestType type) const = 0;

    /// Check if there are any requests pending for any bank
    [[nodiscard]] virtual bool has_any_pending() const = 0;

    // ========================================================================
    // Statistics
    // ========================================================================

    /// Get number of requests selected (lifetime)
    [[nodiscard]] virtual uint64_t requests_selected() const = 0;

    /// Get number of row hits selected
    [[nodiscard]] virtual uint64_t row_hits_selected() const = 0;

    /// Get number of grouping decisions made
    [[nodiscard]] virtual uint64_t grouping_decisions() const = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create a scheduler based on configuration
std::unique_ptr<IScheduler> create_scheduler(const SchedulerConfig& config);

} // namespace sw::memsim
