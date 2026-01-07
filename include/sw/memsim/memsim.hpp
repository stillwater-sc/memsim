#pragma once

/// @file memsim.hpp
/// @brief Single-header convenience include for stillwater-memsim
///
/// This header includes all public interfaces of the stillwater-memsim library.
/// For more granular includes, use the individual headers in the subdirectories.

// Core types and definitions
#include <sw/memsim/core/types.hpp>
#include <sw/memsim/core/timing.hpp>
#include <sw/memsim/core/statistics.hpp>

// Interfaces
#include <sw/memsim/interface/memory_controller.hpp>
#include <sw/memsim/interface/scheduler.hpp>
#include <sw/memsim/interface/refresh_manager.hpp>

/// @namespace sw::memsim
/// @brief Stillwater Memory Simulation Library
///
/// The stillwater-memsim library provides multi-fidelity memory simulation
/// for embodied AI accelerator design space exploration.
///
/// ## Key Features
///
/// - **Multi-fidelity simulation**: Same interface works at behavioral,
///   transactional, and cycle-accurate fidelity levels
/// - **Modern memory technologies**: LPDDR5/5X/6, HBM3/4, GDDR7
/// - **Flexible scheduling**: FR-FCFS, grouping, QoS-aware policies
/// - **Power-aware**: Refresh management, power-down policies
///
/// ## Quick Start
///
/// ```cpp
/// #include <sw/memsim/memsim.hpp>
///
/// using namespace sw::memsim;
///
/// int main() {
///     // Configure LPDDR5 at cycle-accurate fidelity
///     ControllerConfig config;
///     config.technology = Technology::LPDDR5;
///     config.fidelity = Fidelity::CYCLE_ACCURATE;
///     config.speed_mt_s = 6400;
///     config.timing = timing_presets::lpddr5_6400();
///
///     // Create controller
///     auto controller = create_controller(config);
///
///     // Submit requests
///     controller->read(0x1000, 64, [](Cycle latency) {
///         std::cout << "Read completed in " << latency << " cycles\n";
///     });
///
///     // Run simulation
///     controller->drain();
///
///     // Check statistics
///     std::cout << "Avg latency: " << controller->stats().avg_latency() << "\n";
///     std::cout << "Page hit rate: " << controller->stats().page_hit_rate() << "\n";
/// }
/// ```
///
/// ## Fidelity Levels
///
/// | Level | Speed | Accuracy | Use Case |
/// |-------|-------|----------|----------|
/// | BEHAVIORAL | ~1000x | Functional | Algorithm development |
/// | TRANSACTIONAL | ~100x | Statistical | Early design exploration |
/// | CYCLE_ACCURATE | 1x | Protocol | Detailed timing analysis |
///
/// ## Supported Technologies
///
/// - LPDDR5/5X/6 (mobile, edge AI)
/// - HBM3/3E/4 (datacenter AI accelerators)
/// - GDDR6/7 (discrete graphics, inference)
/// - DDR5 (servers, workstations)

namespace sw::memsim {

/// Library version
constexpr struct {
    int major = 0;
    int minor = 1;
    int patch = 0;
    const char* string = "0.1.0";
} version;

} // namespace sw::memsim
