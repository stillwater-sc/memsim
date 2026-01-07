#include <sw/memsim/memsim.hpp>
#include <sw/memsim/technology/lpddr5/lpddr5_controller.hpp>

#include <iostream>
#include <iomanip>

using namespace sw::memsim;

int main() {
    std::cout << "Stillwater MemSim - Simple Simulation Example\n";
    std::cout << "=============================================\n\n";

    // Configure LPDDR5 at behavioral fidelity (fast)
    ControllerConfig config;
    config.technology = Technology::LPDDR5;
    config.fidelity = Fidelity::BEHAVIORAL;
    config.speed_mt_s = 6400;
    config.timing = timing_presets::lpddr5_6400();
    config.timing.fixed_read_latency = 80;
    config.timing.fixed_write_latency = 100;

    // Create controller
    auto controller = lpddr5::create_lpddr5_controller(config);

    std::cout << "Controller: LPDDR5-6400 @ " << to_string(config.fidelity) << "\n";
    std::cout << "Channels: " << static_cast<int>(controller->num_channels()) << "\n";
    std::cout << "Banks/channel: " << static_cast<int>(controller->banks_per_channel()) << "\n\n";

    // Submit some read requests
    std::cout << "Submitting 10 read requests...\n";
    for (int i = 0; i < 10; ++i) {
        uint64_t addr = i * 64;  // Sequential 64-byte reads
        controller->read(addr, 64, [i](Cycle latency) {
            std::cout << "  Read " << i << " completed in " << latency << " cycles\n";
        });
    }

    // Submit some write requests
    std::cout << "\nSubmitting 10 write requests...\n";
    for (int i = 0; i < 10; ++i) {
        uint64_t addr = 0x10000 + i * 64;
        controller->write(addr, 64, [i](Cycle latency) {
            std::cout << "  Write " << i << " completed in " << latency << " cycles\n";
        });
    }

    // Drain (for non-behavioral fidelity)
    controller->drain();

    // Print statistics
    const auto& stats = controller->stats();
    std::cout << "\n--- Statistics ---\n";
    std::cout << "Total requests: " << stats.total_requests() << "\n";
    std::cout << "  Reads: " << stats.reads << "\n";
    std::cout << "  Writes: " << stats.writes << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Avg read latency: " << stats.avg_read_latency() << " cycles\n";
    std::cout << "Avg write latency: " << stats.avg_write_latency() << " cycles\n";
    std::cout << "Page hit rate: " << (stats.page_hit_rate() * 100) << "%\n";

    return 0;
}
