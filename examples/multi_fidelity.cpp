#include <sw/memsim/memsim.hpp>
#include <sw/memsim/technology/lpddr5/lpddr5_controller.hpp>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>

using namespace sw::memsim;

void run_benchmark(IMemoryController& controller, int num_requests, const std::string& name) {
    auto start = std::chrono::high_resolution_clock::now();

    // Submit requests
    for (int i = 0; i < num_requests; ++i) {
        uint64_t addr = (i % 1000) * 64;  // Mix of row hits and misses
        if (i % 2 == 0) {
            controller.read(addr, 64);
        } else {
            controller.write(addr, 64);
        }
    }

    // Drain pending requests
    controller.drain();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    const auto& stats = controller.stats();

    std::cout << std::setw(15) << name << ": ";
    std::cout << std::setw(8) << duration.count() << " us, ";
    std::cout << "avg_lat=" << std::setw(6) << std::fixed << std::setprecision(1)
              << stats.avg_latency() << ", ";
    std::cout << "hit_rate=" << std::setw(5) << std::setprecision(1)
              << (stats.page_hit_rate() * 100) << "%\n";
}

int main() {
    std::cout << "Stillwater MemSim - Multi-Fidelity Comparison\n";
    std::cout << "==============================================\n\n";

    const int NUM_REQUESTS = 10000;

    // Base configuration
    ControllerConfig config;
    config.technology = Technology::LPDDR5;
    config.speed_mt_s = 6400;
    config.timing = timing_presets::lpddr5_6400();

    std::cout << "Running " << NUM_REQUESTS << " requests at each fidelity level...\n\n";

    // Behavioral
    {
        config.fidelity = Fidelity::BEHAVIORAL;
        auto controller = lpddr5::create_lpddr5_controller(config);
        run_benchmark(*controller, NUM_REQUESTS, "BEHAVIORAL");
    }

    // Transactional
    {
        config.fidelity = Fidelity::TRANSACTIONAL;
        auto controller = lpddr5::create_lpddr5_controller(config);
        run_benchmark(*controller, NUM_REQUESTS, "TRANSACTIONAL");
    }

    // Cycle-accurate
    {
        config.fidelity = Fidelity::CYCLE_ACCURATE;
        auto controller = lpddr5::create_lpddr5_controller(config);
        run_benchmark(*controller, NUM_REQUESTS, "CYCLE_ACCURATE");
    }

    std::cout << "\nNote: Cycle-accurate is slower but provides protocol-level accuracy.\n";
    std::cout << "Use behavioral/transactional for early exploration, cycle-accurate for validation.\n";

    return 0;
}
