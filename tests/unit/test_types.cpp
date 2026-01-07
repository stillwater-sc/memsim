#include <catch2/catch_test_macros.hpp>
#include <sw/memsim/memsim.hpp>

using namespace sw::memsim;

TEST_CASE("Fidelity enum to_string", "[types]") {
    REQUIRE(to_string(Fidelity::BEHAVIORAL) == "BEHAVIORAL");
    REQUIRE(to_string(Fidelity::TRANSACTIONAL) == "TRANSACTIONAL");
    REQUIRE(to_string(Fidelity::CYCLE_ACCURATE) == "CYCLE_ACCURATE");
}

TEST_CASE("Technology enum to_string", "[types]") {
    REQUIRE(to_string(Technology::LPDDR5) == "LPDDR5");
    REQUIRE(to_string(Technology::HBM3) == "HBM3");
    REQUIRE(to_string(Technology::GDDR7) == "GDDR7");
}

TEST_CASE("BankState enum to_string", "[types]") {
    REQUIRE(to_string(BankState::IDLE) == "IDLE");
    REQUIRE(to_string(BankState::ACTIVE) == "ACTIVE");
    REQUIRE(to_string(BankState::REFRESHING) == "REFRESHING");
}

TEST_CASE("Statistics default values", "[statistics]") {
    Statistics stats;
    REQUIRE(stats.reads == 0);
    REQUIRE(stats.writes == 0);
    REQUIRE(stats.total_requests() == 0);
    REQUIRE(stats.avg_latency() == 0.0);
}

TEST_CASE("Statistics record_request", "[statistics]") {
    Statistics stats;

    stats.record_request(RequestType::READ, 100, true, false);
    REQUIRE(stats.reads == 1);
    REQUIRE(stats.page_hits == 1);
    REQUIRE(stats.total_read_latency == 100);

    stats.record_request(RequestType::WRITE, 150, false, true);
    REQUIRE(stats.writes == 1);
    REQUIRE(stats.page_conflicts == 1);
    REQUIRE(stats.total_write_latency == 150);

    REQUIRE(stats.total_requests() == 2);
    REQUIRE(stats.avg_latency() == 125.0);
    REQUIRE(stats.page_hit_rate() == 0.5);
}

TEST_CASE("Timing presets", "[timing]") {
    auto lpddr5 = timing_presets::lpddr5_6400();
    REQUIRE(lpddr5.tRCD == 18);
    REQUIRE(lpddr5.tRP == 18);
    REQUIRE(lpddr5.tCL == 17);

    auto hbm3 = timing_presets::hbm3_5600();
    REQUIRE(hbm3.tRCD == 14);
    REQUIRE(hbm3.tBurst == 4);  // HBM uses shorter bursts
}

TEST_CASE("OrganizationParams derived values", "[timing]") {
    OrganizationParams org;
    org.num_channels = 2;
    org.ranks_per_channel = 1;
    org.bank_groups_per_rank = 4;
    org.banks_per_bank_group = 4;

    REQUIRE(org.banks_per_rank() == 16);
    REQUIRE(org.total_banks() == 32);
}

TEST_CASE("ControllerConfig clock calculations", "[timing]") {
    ControllerConfig config;
    config.speed_mt_s = 6400;

    REQUIRE(config.clock_mhz() == 3200);
    REQUIRE(config.clock_period_ps() == 312);  // ~312.5ps
}
