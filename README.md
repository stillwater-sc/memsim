# Stillwater MemSim

Multi-fidelity memory simulation library for embodied AI accelerator design space exploration.

## Features

- **Multi-fidelity simulation**: Same interface works at behavioral (~1000x), transactional (~100x), and cycle-accurate (1x) fidelity levels
- **Modern memory technologies**: LPDDR5/5X/6, HBM3/4, GDDR7, DDR5
- **Flexible scheduling**: FIFO, FR-FCFS, FR-FCFS with R/W grouping, QoS-aware
- **Power-aware**: Refresh management, power-down policies
- **Embeddable**: Header-only option, minimal dependencies

## Quick Start

```cpp
#include <sw/memsim/memsim.hpp>

using namespace sw::memsim;

int main() {
    // Configure LPDDR5 at cycle-accurate fidelity
    ControllerConfig config;
    config.technology = Technology::LPDDR5;
    config.fidelity = Fidelity::CYCLE_ACCURATE;
    config.speed_mt_s = 6400;
    config.timing = timing_presets::lpddr5_6400();

    // Create controller
    auto controller = create_controller(config);

    // Submit requests
    controller->read(0x1000, 64, [](Cycle latency) {
        std::cout << "Read completed in " << latency << " cycles\n";
    });

    // Run simulation
    controller->drain();

    // Check statistics
    std::cout << "Avg latency: " << controller->stats().avg_latency() << "\n";
    std::cout << "Page hit rate: " << controller->stats().page_hit_rate() << "\n";
}
```

## Fidelity Levels

| Level | Speed | Accuracy | Use Case |
|-------|-------|----------|----------|
| BEHAVIORAL | ~1000x | Functional | Algorithm development |
| TRANSACTIONAL | ~100x | Statistical | Early design exploration |
| CYCLE_ACCURATE | 1x | Protocol | Detailed timing analysis |

## Supported Technologies

| Technology | Status | Use Case |
|------------|--------|----------|
| LPDDR5 | Implemented | Mobile, edge AI |
| LPDDR5X | Planned | High-performance mobile |
| LPDDR6 | Planned | Future mobile |
| HBM3 | Planned | Datacenter AI |
| HBM3E | Planned | High-bandwidth AI |
| HBM4 | Planned | Future AI |
| GDDR7 | Planned | Graphics, inference |
| DDR5 | Planned | Servers |

## Building

```bash
# Development build
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Release build
cmake --preset release
cmake --build --preset release
```

## Integration with kpu-sim

This library is designed to integrate with [kpu-sim](https://github.com/stillwater-sc/kpu-sim) for KPU accelerator simulation:

```cpp
#include <sw/memsim/memsim.hpp>
#include <sw/kpu/components/memory_controller.hpp>

// Adapter from memsim to kpu-sim interface
class MemSimAdapter : public sw::kpu::IMemoryController {
    std::unique_ptr<sw::memsim::IMemoryController> impl_;
    // ... delegate methods
};
```

## Architecture

```
IMemoryController (interface)
├── BehavioralController (instant/fixed latency)
├── TransactionalController (queue-based statistical)
└── CycleAccurateController (full protocol)
    ├── IScheduler (FR-FCFS, grouping, QoS)
    ├── IRefreshManager (per-bank, same-bank)
    └── Bank state machines
```

## License

BSD 3-Clause License. See [LICENSE](LICENSE) for details.

## Acknowledgments

- Scheduler algorithms adapted from [DRAMSys](https://github.com/tukl-msd/DRAMSys)
- Multi-fidelity framework inspired by [kpu-sim](https://github.com/stillwater-sc/kpu-sim)
