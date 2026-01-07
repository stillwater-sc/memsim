#pragma once

#include <sw/memsim/core/types.hpp>

#include <memory>
#include <vector>

namespace sw::memsim {

/// Refresh policy types
enum class RefreshPolicy : uint8_t {
    NONE,               ///< No refresh (for SRAM, STT-MRAM)
    ALL_BANK,           ///< Traditional all-bank refresh
    PER_BANK,           ///< Per-bank refresh (LPDDR4/5, HBM)
    SAME_BANK,          ///< Same-bank refresh (DDR5)
    PER_2_BANK,         ///< Per-2-bank refresh
    FINE_GRANULARITY    ///< Fine-granularity refresh (HBM3)
};

constexpr std::string_view to_string(RefreshPolicy p) {
    switch (p) {
        case RefreshPolicy::NONE:             return "NONE";
        case RefreshPolicy::ALL_BANK:         return "ALL_BANK";
        case RefreshPolicy::PER_BANK:         return "PER_BANK";
        case RefreshPolicy::SAME_BANK:        return "SAME_BANK";
        case RefreshPolicy::PER_2_BANK:       return "PER_2_BANK";
        case RefreshPolicy::FINE_GRANULARITY: return "FINE_GRANULARITY";
        default: return "UNKNOWN";
    }
}

/// Refresh manager configuration
struct RefreshConfig {
    RefreshPolicy policy = RefreshPolicy::ALL_BANK;

    uint32_t tREFI = 3900;       ///< Refresh interval (cycles)
    uint32_t tRFC = 280;         ///< All-bank refresh cycle time
    uint32_t tRFCpb = 90;        ///< Per-bank refresh cycle time
    uint32_t tRFCsb = 90;        ///< Same-bank refresh cycle time

    uint8_t max_postpone = 8;    ///< Maximum refresh postponement (multiples of tREFI)
    uint8_t max_pull_in = 8;     ///< Maximum refresh pull-in (for idle periods)

    uint8_t num_banks = 16;      ///< Number of banks to manage
    uint8_t num_ranks = 1;       ///< Number of ranks
};

/// Bank identifier for refresh operations
struct BankId {
    Channel channel = 0;
    Rank rank = 0;
    Bank bank = 0;

    bool operator==(const BankId& other) const {
        return channel == other.channel &&
               rank == other.rank &&
               bank == other.bank;
    }
};

/// Abstract refresh manager interface
///
/// The refresh manager is responsible for:
/// 1. Tracking refresh deadlines for each bank/rank
/// 2. Signaling when refresh is required
/// 3. Managing postponement and pull-in
/// 4. Ensuring data retention guarantees are met
class IRefreshManager {
public:
    virtual ~IRefreshManager() = default;

    // ========================================================================
    // Refresh Status
    // ========================================================================

    /// Check if refresh is required for any bank/rank
    [[nodiscard]] virtual bool refresh_required() const = 0;

    /// Check if refresh is urgent (postponement limit reached)
    [[nodiscard]] virtual bool refresh_urgent() const = 0;

    /// Get the bank(s) that need refresh
    [[nodiscard]] virtual std::vector<BankId> banks_to_refresh() const = 0;

    /// Get refresh cycle time for specified bank(s)
    [[nodiscard]] virtual Cycle refresh_latency(const std::vector<BankId>& banks) const = 0;

    // ========================================================================
    // Refresh Control
    // ========================================================================

    /// Signal that refresh was issued for specified bank(s)
    virtual void refresh_issued(const std::vector<BankId>& banks) = 0;

    /// Advance refresh timing by one cycle
    virtual void tick() = 0;

    /// Check if refresh can be postponed
    [[nodiscard]] virtual bool can_postpone() const = 0;

    /// Postpone refresh by one interval (returns false if limit reached)
    virtual bool postpone() = 0;

    /// Get current postponement count
    [[nodiscard]] virtual unsigned postpone_count() const = 0;

    /// Pull in refresh during idle period
    virtual void pull_in() = 0;

    /// Get current pull-in count
    [[nodiscard]] virtual unsigned pull_in_count() const = 0;

    /// Reset refresh state
    virtual void reset() = 0;

    // ========================================================================
    // Statistics
    // ========================================================================

    /// Get total refresh count
    [[nodiscard]] virtual uint64_t refresh_count() const = 0;

    /// Get total postponements
    [[nodiscard]] virtual uint64_t postpone_total() const = 0;

    /// Get total pull-ins
    [[nodiscard]] virtual uint64_t pull_in_total() const = 0;

    /// Get cycles spent in refresh
    [[nodiscard]] virtual uint64_t refresh_cycles() const = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create a refresh manager based on configuration
std::unique_ptr<IRefreshManager> create_refresh_manager(const RefreshConfig& config);

} // namespace sw::memsim
