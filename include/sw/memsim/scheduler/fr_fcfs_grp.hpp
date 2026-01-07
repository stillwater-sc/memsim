#pragma once

#include <sw/memsim/interface/scheduler.hpp>

#include <algorithm>
#include <list>
#include <vector>

namespace sw::memsim {

/// First-Ready First-Come-First-Served with Read/Write Grouping
///
/// FR-FCFS-GRP extends FR-FCFS by preferring requests of the same type
/// (read or write) as the last issued command. This reduces bus turnaround
/// delays which can be significant (tWTR, tRTW).
///
/// Priority order:
/// 1. Row hit + same command type
/// 2. Row hit + different command type
/// 3. No row hit, FCFS
///
/// The scheduler also detects RAW/WAR hazards on the same address to
/// prevent data corruption.
///
/// Algorithm (adapted from DRAMSys SchedulerFrFcfsGrp):
/// 1. Filter requests that would be row hits
/// 2. Among row hits, prefer same command type as last issued
/// 3. Check for address hazards before selecting
/// 4. Fall back to any row hit, then FCFS
class FrFcfsGrpScheduler : public IScheduler {
public:
    explicit FrFcfsGrpScheduler(const SchedulerConfig& config)
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

        // Track last command type for grouping
        last_command_ = request.type;

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

        if (open_row.has_value()) {
            // Step 1: Filter all row hits
            std::vector<Request*> row_hits;
            for (auto* req : buffer) {
                if (req->row == *open_row) {
                    row_hits.push_back(req);
                }
            }

            if (!row_hits.empty()) {
                // Step 2: Among row hits, prefer same command type (grouping)
                for (auto* req : row_hits) {
                    if (req->type == last_command_) {
                        // Step 3: Check for RAW/WAR hazards
                        if (!has_address_hazard(row_hits, req)) {
                            const_cast<FrFcfsGrpScheduler*>(this)->row_hits_++;
                            const_cast<FrFcfsGrpScheduler*>(this)->grouping_decisions_++;
                            const_cast<FrFcfsGrpScheduler*>(this)->requests_selected_++;
                            return req;
                        }
                    }
                }

                // No same-type hit without hazard, take first row hit
                const_cast<FrFcfsGrpScheduler*>(this)->row_hits_++;
                const_cast<FrFcfsGrpScheduler*>(this)->requests_selected_++;
                return row_hits.front();
            }
        }

        // No row hit found or bank precharged, return oldest (FCFS)
        const_cast<FrFcfsGrpScheduler*>(this)->requests_selected_++;
        return buffer.front();
    }

    [[nodiscard]] bool has_row_hit(Bank bank, Row row, [[maybe_unused]] RequestType type) const override {
        unsigned hit_count = 0;
        for (auto* req : buffers_[bank]) {
            if (req->row == row) {
                hit_count++;
                if (hit_count >= 2) {
                    return true;
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
    [[nodiscard]] uint64_t grouping_decisions() const override { return grouping_decisions_; }

private:
    /// Check for RAW/WAR hazard between candidate and earlier requests
    [[nodiscard]] bool has_address_hazard(
        const std::vector<Request*>& candidates,
        const Request* target) const
    {
        for (auto* req : candidates) {
            if (req == target) {
                break;  // Only check requests before target
            }
            if (req->address == target->address) {
                return true;  // Same address, different request = hazard
            }
        }
        return false;
    }

    SchedulerConfig config_;
    std::vector<std::list<Request*>> buffers_;
    std::vector<unsigned> buffer_depths_;
    size_t total_occupancy_ = 0;

    RequestType last_command_ = RequestType::READ;

    // Statistics
    mutable uint64_t requests_selected_ = 0;
    mutable uint64_t row_hits_ = 0;
    mutable uint64_t grouping_decisions_ = 0;
};

} // namespace sw::memsim
