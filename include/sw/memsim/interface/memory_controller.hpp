#pragma once

#include <sw/memsim/core/types.hpp>
#include <sw/memsim/core/timing.hpp>
#include <sw/memsim/core/statistics.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace sw::memsim {

/// Abstract memory controller interface
///
/// This interface provides a common API that is implemented by:
/// - BehavioralController (instant/fixed latency)
/// - TransactionalController (queue-based statistical timing)
/// - CycleAccurateController (full protocol state machines)
///
/// All implementations guarantee:
/// 1. Functional correctness (data is transferred correctly)
/// 2. Callback semantics (callbacks are invoked when operation completes)
/// 3. Statistics collection (if enabled)
/// 4. Fidelity-appropriate timing behavior
class IMemoryController {
public:
    virtual ~IMemoryController() = default;

    // ========================================================================
    // Request Interface
    // ========================================================================

    /// Submit a memory request
    ///
    /// @param request The memory request to submit
    /// @return Request ID if accepted, nullopt if queue is full
    ///
    /// Behavior by fidelity:
    /// - BEHAVIORAL: Completes immediately, callback invoked before return
    /// - TRANSACTIONAL: Queued, callback invoked after statistical delay
    /// - CYCLE_ACCURATE: Queued, callback invoked after protocol timing
    virtual std::optional<RequestId> submit(Request request) = 0;

    /// Convenience method: submit a read request
    std::optional<RequestId> read(Address address, uint32_t size,
                                   CompletionCallback callback = nullptr) {
        Request req;
        req.address = address;
        req.size = size;
        req.type = RequestType::READ;
        req.callback = std::move(callback);
        return submit(std::move(req));
    }

    /// Convenience method: submit a write request
    std::optional<RequestId> write(Address address, uint32_t size,
                                    CompletionCallback callback = nullptr) {
        Request req;
        req.address = address;
        req.size = size;
        req.type = RequestType::WRITE;
        req.callback = std::move(callback);
        return submit(std::move(req));
    }

    /// Check if request queue can accept more requests
    [[nodiscard]] virtual bool can_accept() const = 0;

    /// Check if there are pending requests
    [[nodiscard]] virtual bool has_pending() const = 0;

    /// Get number of pending requests
    [[nodiscard]] virtual size_t pending_count() const = 0;

    // ========================================================================
    // Simulation Interface
    // ========================================================================

    /// Advance simulation by one cycle
    ///
    /// For BEHAVIORAL: May be a no-op (instant completion)
    /// For TRANSACTIONAL: Updates queue state, may complete requests
    /// For CYCLE_ACCURATE: Full FSM advancement, timing checks
    virtual void tick() = 0;

    /// Advance simulation by N cycles
    virtual void tick(Cycle n) {
        for (Cycle i = 0; i < n; ++i) {
            tick();
        }
    }

    /// Process until all pending requests complete
    ///
    /// Useful for draining the controller at end of simulation
    virtual void drain() = 0;

    /// Reset controller to initial state
    virtual void reset() = 0;

    /// Get current simulation cycle
    [[nodiscard]] virtual Cycle cycle() const = 0;

    /// Set current simulation cycle (for external clock management)
    virtual void set_cycle(Cycle cycle) = 0;

    // ========================================================================
    // Configuration Queries
    // ========================================================================

    /// Get simulation fidelity level
    [[nodiscard]] virtual Fidelity fidelity() const = 0;

    /// Get memory technology
    [[nodiscard]] virtual Technology technology() const = 0;

    /// Get full configuration
    [[nodiscard]] virtual const ControllerConfig& config() const = 0;

    // ========================================================================
    // Bank State Queries
    // ========================================================================

    /// Get state of a specific bank
    ///
    /// For BEHAVIORAL/TRANSACTIONAL: Returns simplified state
    /// For CYCLE_ACCURATE: Returns actual bank state
    [[nodiscard]] virtual BankState bank_state(Channel channel, Bank bank) const = 0;

    /// Check if a specific row is open in a bank
    [[nodiscard]] virtual bool is_row_open(Channel channel, Bank bank, Row row) const = 0;

    /// Get the currently open row in a bank (if any)
    [[nodiscard]] virtual std::optional<Row> open_row(Channel channel, Bank bank) const = 0;

    /// Get number of channels
    [[nodiscard]] virtual Channel num_channels() const = 0;

    /// Get number of banks per channel
    [[nodiscard]] virtual Bank banks_per_channel() const = 0;

    // ========================================================================
    // Statistics
    // ========================================================================

    /// Get current statistics
    [[nodiscard]] virtual const Statistics& stats() const = 0;

    /// Get mutable statistics (for testing/debugging)
    [[nodiscard]] virtual Statistics& stats() = 0;

    /// Reset statistics
    virtual void reset_stats() = 0;

    // ========================================================================
    // Observability
    // ========================================================================

    /// Enable or disable tracing
    virtual void enable_tracing(bool enable) = 0;

    /// Check if tracing is enabled
    [[nodiscard]] virtual bool tracing_enabled() const = 0;

    /// Enable or disable invariant checking
    virtual void enable_invariants(bool enable) = 0;

    /// Check if invariant checking is enabled
    [[nodiscard]] virtual bool invariants_enabled() const = 0;

    // ========================================================================
    // Invariant Checking (CYCLE_ACCURATE only)
    // ========================================================================

    /// Invariant violation record
    struct Violation {
        Cycle cycle;
        std::string invariant_id;
        std::string message;
        Channel channel;
        Bank bank;
    };

    /// Get list of invariant violations
    [[nodiscard]] virtual const std::vector<Violation>& violations() const = 0;

    /// Check if any violations occurred
    [[nodiscard]] virtual bool has_violations() const = 0;

    /// Clear violation list
    virtual void clear_violations() = 0;
};

// ============================================================================
// Factory Function
// ============================================================================

/// Create a memory controller based on configuration
///
/// The factory selects the appropriate implementation based on:
/// 1. Fidelity level (BEHAVIORAL, TRANSACTIONAL, CYCLE_ACCURATE)
/// 2. Memory technology (LPDDR5, HBM3, GDDR7, etc.)
///
/// @param config Memory controller configuration
/// @return Unique pointer to memory controller implementation
std::unique_ptr<IMemoryController> create_controller(const ControllerConfig& config);

} // namespace sw::memsim
