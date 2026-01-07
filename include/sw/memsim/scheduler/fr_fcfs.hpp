#pragma once

#include <sw/memsim/interface/scheduler.hpp>

#include <algorithm>
#include <list>
#include <vector>

namespace sw::memsim {

/// First-Ready First-Come-First-Served Scheduler
///
/// FR-FCFS prioritizes requests that hit in the row buffer (First-Ready),
/// falling back to FCFS ordering when no row hit is available.
///
/// This is the most common DRAM scheduling policy as it provides a good
/// balance between:
/// - Throughput (maximizing row buffer hits)
/// - Fairness (FCFS for equal-priority requests)
/// - Simplicity (low hardware complexity)
///
/// Algorithm (adapted from DRAMSys):
/// 1. If bank is activated, search for row hit
/// 2. If row hit found, return it
/// 3. Otherwise, return oldest request (FCFS)
class FrFcfsScheduler : public IScheduler {
public:
    explicit FrFcfsScheduler(const SchedulerConfig& config)
        : config_(config)
        , buffers_(config.num_banks)
        , buffer_depths_(config.num_banks, 0)
    {}

    // ========================================================================
    // Buffer Management
    // ========================================================================

    [[nodiscard]] bool has_space(unsigned count) const override {
        return total_occupancy_ + count <= config_.buffer_size;
    }

    void store(Request& request) override {
        Bank bank = request.bank;
        buffers_[bank].push_back(&request);
        buffer_depths_[bank]++;
        total_occupancy_++;
    }

    void remove(const Request& request) override {
        Bank bank = request.bank;
        auto& buffer = buffers_[bank];

        for (auto it = buffer.begin(); it != buffer.end(); ++it) {
            if ((*it)->id == request.id) {
                buffer.erase(it);
                buffer_depths_[bank]--;
                total_occupancy_--;
                return;
            }
        }
    }

    [[nodiscard]] size_t occupancy() const override {
        return total_occupancy_;
    }

    [[nodiscard]] std::span<const unsigned> buffer_depth() const override {
        return buffer_depths_;
    }

    // ========================================================================
    // Request Selection
    // ========================================================================

    [[nodiscard]] Request* get_next(
        Bank bank,
        std::optional<Row> open_row,
        [[maybe_unused]] RequestType last_cmd) const override
    {
        auto& buffer = buffers_[bank];
        if (buffer.empty()) {
            return nullptr;
        }

        // If bank has an open row, search for row hit
        if (open_row.has_value()) {
            for (auto* req : buffer) {
                if (req->row == *open_row) {
                    // Found row hit
                    const_cast<FrFcfsScheduler*>(this)->row_hits_++;
                    const_cast<FrFcfsScheduler*>(this)->requests_selected_++;
                    return req;
                }
            }
        }

        // No row hit found or bank precharged, return oldest (FCFS)
        const_cast<FrFcfsScheduler*>(this)->requests_selected_++;
        return buffer.front();
    }

    [[nodiscard]] bool has_row_hit(Bank bank, Row row, [[maybe_unused]] RequestType type) const override {
        unsigned hit_count = 0;
        for (auto* req : buffers_[bank]) {
            if (req->row == row) {
                hit_count++;
                if (hit_count >= 2) {
                    return true;  // At least one more hit after current
                }
            }
        }
        return false;
    }

    [[nodiscard]] bool has_pending(Bank bank, [[maybe_unused]] RequestType type) const override {
        return buffers_[bank].size() >= 2;
    }

    [[nodiscard]] bool has_any_pending() const override {
        return total_occupancy_ > 0;
    }

    // ========================================================================
    // Statistics
    // ========================================================================

    [[nodiscard]] uint64_t requests_selected() const override { return requests_selected_; }
    [[nodiscard]] uint64_t row_hits_selected() const override { return row_hits_; }
    [[nodiscard]] uint64_t grouping_decisions() const override { return 0; }  // N/A for FR-FCFS

private:
    SchedulerConfig config_;
    std::vector<std::list<Request*>> buffers_;
    std::vector<unsigned> buffer_depths_;
    size_t total_occupancy_ = 0;

    // Statistics
    mutable uint64_t requests_selected_ = 0;
    mutable uint64_t row_hits_ = 0;
};

} // namespace sw::memsim
