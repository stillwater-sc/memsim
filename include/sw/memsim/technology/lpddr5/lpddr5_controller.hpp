#pragma once

#include <sw/memsim/interface/memory_controller.hpp>
#include <sw/memsim/interface/scheduler.hpp>
#include <sw/memsim/interface/refresh_manager.hpp>
#include <sw/memsim/scheduler/fr_fcfs.hpp>

#include <array>
#include <queue>
#include <random>

namespace sw::memsim::lpddr5 {

/// LPDDR5-specific timing parameters
struct LPDDR5Timing : public TimingParams {
    // LPDDR5-specific parameters
    uint32_t tWCK2DQO = 2;    ///< WCK to DQ output delay
    uint32_t tWCK2DQI = 2;    ///< WCK to DQ input delay
    uint32_t tWCKPST = 2;     ///< WCK post-amble
    uint32_t tWCKPRE = 2;     ///< WCK pre-amble

    /// Create timing for speed grade (MT/s)
    static LPDDR5Timing from_speed(uint32_t speed_mt_s);
};

/// LPDDR5 bank state machine
struct LPDDR5Bank {
    BankState state = BankState::IDLE;
    Row open_row = 0;
    Cycle state_until = 0;     ///< Cycle when current state completes

    // Timing constraints
    Cycle next_act = 0;        ///< Earliest cycle for ACT
    Cycle next_rd = 0;         ///< Earliest cycle for RD
    Cycle next_wr = 0;         ///< Earliest cycle for WR
    Cycle next_pre = 0;        ///< Earliest cycle for PRE

    bool is_ready_for(RequestType type, Cycle now) const {
        if (state != BankState::ACTIVE) return false;
        return (type == RequestType::READ) ? (now >= next_rd) : (now >= next_wr);
    }
};

// ============================================================================
// Behavioral LPDDR5 Controller
// ============================================================================

/// Behavioral LPDDR5 controller (instant/fixed latency)
class BehavioralLPDDR5Controller : public IMemoryController {
public:
    explicit BehavioralLPDDR5Controller(const ControllerConfig& config)
        : config_(config)
    {}

    std::optional<RequestId> submit(Request request) override {
        request.id = next_id_++;
        Cycle latency = (request.type == RequestType::READ)
            ? config_.timing.fixed_read_latency
            : config_.timing.fixed_write_latency;

        if (request.type == RequestType::READ) {
            stats_.reads++;
            stats_.total_read_latency += latency;
        } else {
            stats_.writes++;
            stats_.total_write_latency += latency;
        }

        if (request.callback) {
            request.callback(latency);
        }
        return request.id;
    }

    [[nodiscard]] bool can_accept() const override { return true; }
    [[nodiscard]] bool has_pending() const override { return false; }
    [[nodiscard]] size_t pending_count() const override { return 0; }

    void tick() override { current_cycle_++; }
    void drain() override {}
    void reset() override { current_cycle_ = 0; stats_.reset(); }

    [[nodiscard]] Cycle cycle() const override { return current_cycle_; }
    void set_cycle(Cycle c) override { current_cycle_ = c; }

    [[nodiscard]] Fidelity fidelity() const override { return Fidelity::BEHAVIORAL; }
    [[nodiscard]] Technology technology() const override { return Technology::LPDDR5; }
    [[nodiscard]] const ControllerConfig& config() const override { return config_; }

    [[nodiscard]] BankState bank_state(Channel, Bank) const override { return BankState::ACTIVE; }
    [[nodiscard]] bool is_row_open(Channel, Bank, Row) const override { return true; }
    [[nodiscard]] std::optional<Row> open_row(Channel, Bank) const override { return 0; }
    [[nodiscard]] Channel num_channels() const override { return config_.organization.num_channels; }
    [[nodiscard]] Bank banks_per_channel() const override { return config_.organization.banks_per_rank(); }

    [[nodiscard]] const Statistics& stats() const override { return stats_; }
    [[nodiscard]] Statistics& stats() override { return stats_; }
    void reset_stats() override { stats_.reset(); }

    void enable_tracing(bool e) override { tracing_ = e; }
    [[nodiscard]] bool tracing_enabled() const override { return tracing_; }
    void enable_invariants(bool) override {}
    [[nodiscard]] bool invariants_enabled() const override { return false; }

    [[nodiscard]] const std::vector<Violation>& violations() const override { return violations_; }
    [[nodiscard]] bool has_violations() const override { return false; }
    void clear_violations() override {}

private:
    ControllerConfig config_;
    Cycle current_cycle_ = 0;
    RequestId next_id_ = 1;
    Statistics stats_;
    bool tracing_ = false;
    std::vector<Violation> violations_;
};

// ============================================================================
// Transactional LPDDR5 Controller
// ============================================================================

/// Transactional LPDDR5 controller (queue-based statistical timing)
class TransactionalLPDDR5Controller : public IMemoryController {
public:
    explicit TransactionalLPDDR5Controller(const ControllerConfig& config)
        : config_(config)
        , rng_(std::random_device{}())
        , latency_dist_(config.timing.mean_read_latency, config.timing.latency_stddev)
    {}

    std::optional<RequestId> submit(Request request) override {
        if (pending_.size() >= config_.queue_depth) {
            return std::nullopt;
        }

        request.id = next_id_++;
        request.submit_cycle = current_cycle_;

        // Estimate latency based on page hit probability
        Cycle latency = estimate_latency(request);
        pending_.push({request, current_cycle_ + latency});

        return request.id;
    }

    [[nodiscard]] bool can_accept() const override {
        return pending_.size() < config_.queue_depth;
    }

    [[nodiscard]] bool has_pending() const override { return !pending_.empty(); }
    [[nodiscard]] size_t pending_count() const override { return pending_.size(); }

    void tick() override {
        current_cycle_++;

        // Complete requests whose time has come
        while (!pending_.empty() && pending_.front().complete_cycle <= current_cycle_) {
            auto& pr = pending_.front();
            Cycle latency = current_cycle_ - pr.request.submit_cycle;

            if (pr.request.type == RequestType::READ) {
                stats_.reads++;
                stats_.total_read_latency += latency;
            } else {
                stats_.writes++;
                stats_.total_write_latency += latency;
            }

            if (pr.request.callback) {
                pr.request.callback(latency);
            }
            pending_.pop();
        }
    }

    void drain() override {
        while (!pending_.empty()) {
            tick();
        }
    }

    void reset() override {
        current_cycle_ = 0;
        while (!pending_.empty()) pending_.pop();
        stats_.reset();
    }

    [[nodiscard]] Cycle cycle() const override { return current_cycle_; }
    void set_cycle(Cycle c) override { current_cycle_ = c; }

    [[nodiscard]] Fidelity fidelity() const override { return Fidelity::TRANSACTIONAL; }
    [[nodiscard]] Technology technology() const override { return Technology::LPDDR5; }
    [[nodiscard]] const ControllerConfig& config() const override { return config_; }

    [[nodiscard]] BankState bank_state(Channel, Bank) const override { return BankState::ACTIVE; }
    [[nodiscard]] bool is_row_open(Channel, Bank, Row) const override { return true; }
    [[nodiscard]] std::optional<Row> open_row(Channel, Bank) const override { return std::nullopt; }
    [[nodiscard]] Channel num_channels() const override { return config_.organization.num_channels; }
    [[nodiscard]] Bank banks_per_channel() const override { return config_.organization.banks_per_rank(); }

    [[nodiscard]] const Statistics& stats() const override { return stats_; }
    [[nodiscard]] Statistics& stats() override { return stats_; }
    void reset_stats() override { stats_.reset(); }

    void enable_tracing(bool e) override { tracing_ = e; }
    [[nodiscard]] bool tracing_enabled() const override { return tracing_; }
    void enable_invariants(bool) override {}
    [[nodiscard]] bool invariants_enabled() const override { return false; }

    [[nodiscard]] const std::vector<Violation>& violations() const override { return violations_; }
    [[nodiscard]] bool has_violations() const override { return false; }
    void clear_violations() override {}

private:
    struct PendingRequest {
        Request request;
        Cycle complete_cycle;

        bool operator>(const PendingRequest& other) const {
            return complete_cycle > other.complete_cycle;
        }
    };

    Cycle estimate_latency(const Request& req) {
        // Simple statistical model
        double base = (req.type == RequestType::READ)
            ? config_.timing.mean_read_latency
            : config_.timing.mean_write_latency;

        // Add variance
        double latency = base + latency_dist_(rng_);
        return static_cast<Cycle>(std::max(1.0, latency));
    }

    ControllerConfig config_;
    Cycle current_cycle_ = 0;
    RequestId next_id_ = 1;
    std::queue<PendingRequest> pending_;
    Statistics stats_;
    bool tracing_ = false;
    std::vector<Violation> violations_;

    std::mt19937 rng_;
    std::normal_distribution<double> latency_dist_;
};

// ============================================================================
// Cycle-Accurate LPDDR5 Controller
// ============================================================================

/// Cycle-accurate LPDDR5 controller (full protocol state machines)
///
/// This controller implements:
/// - Full LPDDR5 timing constraints
/// - Per-bank state machines
/// - FR-FCFS scheduling (configurable)
/// - Per-bank refresh
/// - Power-down (optional)
class CycleAccurateLPDDR5Controller : public IMemoryController {
public:
    explicit CycleAccurateLPDDR5Controller(const ControllerConfig& config);

    std::optional<RequestId> submit(Request request) override;

    [[nodiscard]] bool can_accept() const override;
    [[nodiscard]] bool has_pending() const override;
    [[nodiscard]] size_t pending_count() const override;

    void tick() override;
    void drain() override;
    void reset() override;

    [[nodiscard]] Cycle cycle() const override { return current_cycle_; }
    void set_cycle(Cycle c) override { current_cycle_ = c; }

    [[nodiscard]] Fidelity fidelity() const override { return Fidelity::CYCLE_ACCURATE; }
    [[nodiscard]] Technology technology() const override { return Technology::LPDDR5; }
    [[nodiscard]] const ControllerConfig& config() const override { return config_; }

    [[nodiscard]] BankState bank_state(Channel channel, Bank bank) const override;
    [[nodiscard]] bool is_row_open(Channel channel, Bank bank, Row row) const override;
    [[nodiscard]] std::optional<Row> open_row(Channel channel, Bank bank) const override;
    [[nodiscard]] Channel num_channels() const override { return config_.organization.num_channels; }
    [[nodiscard]] Bank banks_per_channel() const override { return config_.organization.banks_per_rank(); }

    [[nodiscard]] const Statistics& stats() const override { return stats_; }
    [[nodiscard]] Statistics& stats() override { return stats_; }
    void reset_stats() override { stats_.reset(); }

    void enable_tracing(bool e) override { tracing_ = e; }
    [[nodiscard]] bool tracing_enabled() const override { return tracing_; }
    void enable_invariants(bool e) override { check_invariants_ = e; }
    [[nodiscard]] bool invariants_enabled() const override { return check_invariants_; }

    [[nodiscard]] const std::vector<Violation>& violations() const override { return violations_; }
    [[nodiscard]] bool has_violations() const override { return !violations_.empty(); }
    void clear_violations() override { violations_.clear(); }

private:
    void decode_address(Request& request);
    void update_bank_states();
    void issue_commands();
    void complete_transfers();
    void check_timing_invariants();

    ControllerConfig config_;
    Cycle current_cycle_ = 0;
    RequestId next_id_ = 1;

    std::vector<LPDDR5Bank> banks_;
    std::unique_ptr<IScheduler> scheduler_;
    std::unique_ptr<IRefreshManager> refresh_;

    RequestType last_command_ = RequestType::READ;
    Cycle last_read_cycle_ = 0;
    Cycle last_write_cycle_ = 0;

    Statistics stats_;
    bool tracing_ = false;
    bool check_invariants_ = false;
    std::vector<Violation> violations_;
};

// ============================================================================
// Factory
// ============================================================================

/// Create LPDDR5 controller based on fidelity level
inline std::unique_ptr<IMemoryController> create_lpddr5_controller(
    const ControllerConfig& config)
{
    switch (config.fidelity) {
        case Fidelity::BEHAVIORAL:
            return std::make_unique<BehavioralLPDDR5Controller>(config);
        case Fidelity::TRANSACTIONAL:
            return std::make_unique<TransactionalLPDDR5Controller>(config);
        case Fidelity::CYCLE_ACCURATE:
            return std::make_unique<CycleAccurateLPDDR5Controller>(config);
        default:
            return nullptr;
    }
}

} // namespace sw::memsim::lpddr5
