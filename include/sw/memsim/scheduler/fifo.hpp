#pragma once

#include <sw/memsim/interface/scheduler.hpp>

#include <list>
#include <vector>

namespace sw::memsim {

/// Simple FIFO Scheduler
///
/// The simplest scheduling policy: requests are served in the order
/// they arrive, with no consideration for row buffer state.
///
/// Advantages:
/// - Maximum fairness (no starvation)
/// - Lowest hardware complexity
/// - Deterministic latency (good for real-time)
///
/// Disadvantages:
/// - Poor row buffer utilization
/// - Lower throughput than FR-FCFS
class FifoScheduler : public IScheduler {
public:
    explicit FifoScheduler(const SchedulerConfig& config)
        : config_(config)
        , buffers_(config.num_banks)
        , buffer_depths_(config.num_banks, 0)
    {}

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

    [[nodiscard]] Request* get_next(
        Bank bank,
        [[maybe_unused]] std::optional<Row> open_row,
        [[maybe_unused]] RequestType last_cmd) const override
    {
        auto& buffer = buffers_[bank];
        if (buffer.empty()) {
            return nullptr;
        }
        const_cast<FifoScheduler*>(this)->requests_selected_++;
        return buffer.front();
    }

    [[nodiscard]] bool has_row_hit(
        [[maybe_unused]] Bank bank,
        [[maybe_unused]] Row row,
        [[maybe_unused]] RequestType type) const override
    {
        return false;  // FIFO doesn't track row hits
    }

    [[nodiscard]] bool has_pending(Bank bank, [[maybe_unused]] RequestType type) const override {
        return buffers_[bank].size() >= 2;
    }

    [[nodiscard]] bool has_any_pending() const override {
        return total_occupancy_ > 0;
    }

    [[nodiscard]] uint64_t requests_selected() const override { return requests_selected_; }
    [[nodiscard]] uint64_t row_hits_selected() const override { return 0; }
    [[nodiscard]] uint64_t grouping_decisions() const override { return 0; }

private:
    SchedulerConfig config_;
    std::vector<std::list<Request*>> buffers_;
    std::vector<unsigned> buffer_depths_;
    size_t total_occupancy_ = 0;
    mutable uint64_t requests_selected_ = 0;
};

} // namespace sw::memsim
