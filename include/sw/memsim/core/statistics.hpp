#pragma once

#include <cstdint>
#include <limits>

namespace sw::memsim {

/// Memory controller statistics
struct Statistics {
    // Request counts
    uint64_t reads = 0;
    uint64_t writes = 0;

    // Page/row buffer statistics
    uint64_t page_hits = 0;
    uint64_t page_empty = 0;      ///< Access to closed bank
    uint64_t page_conflicts = 0;  ///< Different row was open

    // Latency statistics (in cycles)
    uint64_t total_read_latency = 0;
    uint64_t total_write_latency = 0;
    uint64_t min_latency = std::numeric_limits<uint64_t>::max();
    uint64_t max_latency = 0;

    // Utilization (in cycles)
    uint64_t busy_cycles = 0;
    uint64_t idle_cycles = 0;
    uint64_t stall_cycles = 0;

    // Refresh statistics
    uint64_t refreshes = 0;
    uint64_t refresh_cycles = 0;

    // Turnaround statistics
    uint64_t read_to_write_turnarounds = 0;
    uint64_t write_to_read_turnarounds = 0;

    // Power statistics
    uint64_t active_cycles = 0;
    uint64_t precharge_cycles = 0;
    uint64_t powerdown_cycles = 0;

    // === Derived Metrics ===

    uint64_t total_requests() const {
        return reads + writes;
    }

    double avg_read_latency() const {
        return reads > 0 ? static_cast<double>(total_read_latency) / reads : 0.0;
    }

    double avg_write_latency() const {
        return writes > 0 ? static_cast<double>(total_write_latency) / writes : 0.0;
    }

    double avg_latency() const {
        uint64_t total = reads + writes;
        return total > 0
            ? static_cast<double>(total_read_latency + total_write_latency) / total
            : 0.0;
    }

    double page_hit_rate() const {
        uint64_t total = page_hits + page_empty + page_conflicts;
        return total > 0 ? static_cast<double>(page_hits) / total : 0.0;
    }

    double page_conflict_rate() const {
        uint64_t total = page_hits + page_empty + page_conflicts;
        return total > 0 ? static_cast<double>(page_conflicts) / total : 0.0;
    }

    double utilization() const {
        uint64_t total = busy_cycles + idle_cycles;
        return total > 0 ? static_cast<double>(busy_cycles) / total : 0.0;
    }

    double read_ratio() const {
        uint64_t total = reads + writes;
        return total > 0 ? static_cast<double>(reads) / total : 0.0;
    }

    /// Reset all statistics to zero
    void reset() {
        reads = writes = 0;
        page_hits = page_empty = page_conflicts = 0;
        total_read_latency = total_write_latency = 0;
        min_latency = std::numeric_limits<uint64_t>::max();
        max_latency = 0;
        busy_cycles = idle_cycles = stall_cycles = 0;
        refreshes = refresh_cycles = 0;
        read_to_write_turnarounds = write_to_read_turnarounds = 0;
        active_cycles = precharge_cycles = powerdown_cycles = 0;
    }

    /// Merge statistics from another instance
    void merge(const Statistics& other) {
        reads += other.reads;
        writes += other.writes;
        page_hits += other.page_hits;
        page_empty += other.page_empty;
        page_conflicts += other.page_conflicts;
        total_read_latency += other.total_read_latency;
        total_write_latency += other.total_write_latency;
        min_latency = std::min(min_latency, other.min_latency);
        max_latency = std::max(max_latency, other.max_latency);
        busy_cycles += other.busy_cycles;
        idle_cycles += other.idle_cycles;
        stall_cycles += other.stall_cycles;
        refreshes += other.refreshes;
        refresh_cycles += other.refresh_cycles;
        read_to_write_turnarounds += other.read_to_write_turnarounds;
        write_to_read_turnarounds += other.write_to_read_turnarounds;
        active_cycles += other.active_cycles;
        precharge_cycles += other.precharge_cycles;
        powerdown_cycles += other.powerdown_cycles;
    }

    /// Record a completed request
    void record_request(RequestType type, Cycle latency, bool page_hit, bool page_conflict) {
        if (type == RequestType::READ) {
            reads++;
            total_read_latency += latency;
        } else {
            writes++;
            total_write_latency += latency;
        }

        if (page_hit) {
            page_hits++;
        } else if (page_conflict) {
            page_conflicts++;
        } else {
            page_empty++;
        }

        min_latency = std::min(min_latency, latency);
        max_latency = std::max(max_latency, latency);
    }
};

} // namespace sw::memsim
