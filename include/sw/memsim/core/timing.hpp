#pragma once

#include <cstdint>

namespace sw::memsim {

/// DRAM timing parameters (in memory clock cycles)
/// Applicable to DDR5, LPDDR5/5X, HBM3, GDDR7 with technology-specific defaults
struct TimingParams {
    // === Core Timing ===
    uint32_t tRCD = 14;      ///< Row-to-column delay (ACT to RD/WR)
    uint32_t tRP = 14;       ///< Row precharge time (PRE to ACT)
    uint32_t tRAS = 28;      ///< Row active time (minimum ACT to PRE)
    uint32_t tRC = 42;       ///< Row cycle time (ACT to ACT same bank)
    uint32_t tCL = 14;       ///< CAS latency (RD to data out)
    uint32_t tWL = 8;        ///< CAS write latency (WR to data in)
    uint32_t tWR = 24;       ///< Write recovery time (data in to PRE)
    uint32_t tRTP = 6;       ///< Read to precharge time

    // === Bank/Bank Group Timing ===
    uint32_t tRRD_L = 6;     ///< ACT to ACT (same bank group)
    uint32_t tRRD_S = 4;     ///< ACT to ACT (different bank group)
    uint32_t tCCD_L = 6;     ///< CAS to CAS (same bank group)
    uint32_t tCCD_S = 4;     ///< CAS to CAS (different bank group)
    uint32_t tFAW = 24;      ///< Four activate window

    // === Turnaround Timing ===
    uint32_t tWTR_L = 10;    ///< Write to read (same bank group)
    uint32_t tWTR_S = 4;     ///< Write to read (different bank group)
    uint32_t tRTW = 14;      ///< Read to write (bus turnaround)

    // === Burst Timing ===
    uint32_t tBurst = 8;     ///< Burst length in cycles (BL16 / 2)

    // === Refresh Timing ===
    uint32_t tRFC = 280;     ///< Refresh cycle time (all-bank)
    uint32_t tRFCpb = 90;    ///< Refresh cycle time (per-bank)
    uint32_t tRFCsb = 90;    ///< Refresh cycle time (same-bank, DDR5)
    uint32_t tREFI = 3900;   ///< Refresh interval

    // === Power Down Timing ===
    uint32_t tCKE = 5;       ///< CKE minimum pulse width
    uint32_t tXP = 6;        ///< Exit power-down to valid command
    uint32_t tXS = 216;      ///< Exit self-refresh to valid command

    // === Mode Register Timing ===
    uint32_t tMRD = 8;       ///< Mode register set command cycle time
    uint32_t tMOD = 15;      ///< Mode register set to non-MRS command

    // === Behavioral/Transactional Model Parameters ===
    uint32_t fixed_read_latency = 100;   ///< Fixed latency for BEHAVIORAL
    uint32_t fixed_write_latency = 100;

    uint32_t mean_read_latency = 80;     ///< Mean latency for TRANSACTIONAL
    uint32_t mean_write_latency = 90;
    uint32_t latency_stddev = 20;        ///< Standard deviation

    double page_hit_factor = 0.7;        ///< Latency multiplier for page hits
    double page_empty_factor = 1.0;      ///< Latency multiplier for page empty
    double page_conflict_factor = 1.3;   ///< Latency multiplier for page conflicts
};

/// Organization parameters
struct OrganizationParams {
    uint8_t num_channels = 1;
    uint8_t ranks_per_channel = 1;
    uint8_t bank_groups_per_rank = 4;
    uint8_t banks_per_bank_group = 4;

    uint32_t rows_per_bank = 65536;      ///< 64K rows (16-bit row address)
    uint32_t columns_per_row = 1024;     ///< 1K columns (10-bit column address)

    uint8_t device_width = 16;           ///< x16 device
    uint8_t devices_per_rank = 1;        ///< For LPDDR5 (x16)
    uint32_t burst_length = 16;          ///< BL16

    // Derived values
    uint8_t banks_per_rank() const {
        return bank_groups_per_rank * banks_per_bank_group;
    }

    uint8_t total_banks() const {
        return num_channels * ranks_per_channel * banks_per_rank();
    }

    uint64_t channel_capacity_bytes() const {
        return static_cast<uint64_t>(ranks_per_channel) *
               banks_per_rank() *
               rows_per_bank *
               columns_per_row *
               (device_width / 8) *
               devices_per_rank;
    }

    uint64_t total_capacity_bytes() const {
        return num_channels * channel_capacity_bytes();
    }
};

/// Complete memory controller configuration
struct ControllerConfig {
    Technology technology = Technology::IDEAL;
    Fidelity fidelity = Fidelity::BEHAVIORAL;

    uint32_t speed_mt_s = 6400;          ///< Data rate in MT/s
    uint32_t queue_depth = 32;           ///< Request queue depth

    TimingParams timing;
    OrganizationParams organization;

    // Address mapping
    AddressMapping address_mapping = AddressMapping::ROW_BANK_COLUMN;

    // Observability
    bool enable_tracing = false;
    bool enable_statistics = true;
    bool enable_invariants = false;

    /// Get memory clock frequency in MHz (speed / 2 for DDR)
    uint32_t clock_mhz() const {
        return speed_mt_s / 2;
    }

    /// Get clock period in picoseconds
    uint32_t clock_period_ps() const {
        return 1'000'000 / clock_mhz();
    }
};

// ============================================================================
// Technology-Specific Timing Presets
// ============================================================================

namespace timing_presets {

/// LPDDR5-6400 timing parameters
inline TimingParams lpddr5_6400() {
    TimingParams t;
    t.tRCD = 18;
    t.tRP = 18;
    t.tRAS = 42;
    t.tRC = 60;
    t.tCL = 17;
    t.tWL = 8;
    t.tWR = 34;
    t.tRTP = 12;
    t.tRRD_L = 8;
    t.tRRD_S = 4;
    t.tCCD_L = 8;
    t.tCCD_S = 4;
    t.tFAW = 32;
    t.tWTR_L = 16;
    t.tWTR_S = 8;
    t.tRTW = 18;
    t.tBurst = 8;
    t.tRFC = 280;
    t.tRFCpb = 90;
    t.tREFI = 3900;
    return t;
}

/// LPDDR5X-8533 timing parameters
inline TimingParams lpddr5x_8533() {
    TimingParams t = lpddr5_6400();
    t.tRCD = 24;
    t.tRP = 24;
    t.tRAS = 56;
    t.tRC = 80;
    t.tCL = 22;
    t.tWL = 11;
    t.tWR = 45;
    t.tRTP = 16;
    return t;
}

/// HBM3-5600 timing parameters
inline TimingParams hbm3_5600() {
    TimingParams t;
    t.tRCD = 14;
    t.tRP = 14;
    t.tRAS = 28;
    t.tRC = 42;
    t.tCL = 14;
    t.tWL = 4;
    t.tWR = 16;
    t.tRTP = 4;
    t.tRRD_L = 4;
    t.tRRD_S = 4;
    t.tCCD_L = 4;
    t.tCCD_S = 2;
    t.tFAW = 16;
    t.tWTR_L = 8;
    t.tWTR_S = 4;
    t.tRTW = 14;
    t.tBurst = 4;  // HBM uses shorter bursts
    t.tRFC = 280;
    t.tRFCpb = 90;
    t.tREFI = 1950;  // Higher temp, more frequent refresh
    return t;
}

/// GDDR7-32000 timing parameters
inline TimingParams gddr7_32000() {
    TimingParams t;
    t.tRCD = 20;
    t.tRP = 20;
    t.tRAS = 46;
    t.tRC = 66;
    t.tCL = 20;
    t.tWL = 10;
    t.tWR = 28;
    t.tRTP = 10;
    t.tRRD_L = 6;
    t.tRRD_S = 4;
    t.tCCD_L = 4;
    t.tCCD_S = 2;
    t.tFAW = 24;
    t.tWTR_L = 12;
    t.tWTR_S = 6;
    t.tRTW = 16;
    t.tBurst = 8;
    t.tRFC = 350;
    t.tREFI = 1950;
    return t;
}

} // namespace timing_presets

} // namespace sw::memsim
