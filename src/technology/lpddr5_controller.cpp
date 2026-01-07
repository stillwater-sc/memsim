#include <sw/memsim/technology/lpddr5/lpddr5_controller.hpp>

namespace sw::memsim::lpddr5 {

// ============================================================================
// LPDDR5 Timing Presets
// ============================================================================

LPDDR5Timing LPDDR5Timing::from_speed(uint32_t speed_mt_s) {
    LPDDR5Timing t;

    switch (speed_mt_s) {
        case 6400:
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
            break;

        case 7500:
            t.tRCD = 21;
            t.tRP = 21;
            t.tRAS = 49;
            t.tRC = 70;
            t.tCL = 20;
            t.tWL = 10;
            t.tWR = 40;
            t.tRTP = 14;
            t.tRRD_L = 9;
            t.tRRD_S = 5;
            t.tCCD_L = 9;
            t.tCCD_S = 5;
            t.tFAW = 37;
            t.tWTR_L = 19;
            t.tWTR_S = 9;
            t.tRTW = 21;
            t.tBurst = 8;
            t.tRFC = 280;
            t.tRFCpb = 90;
            t.tREFI = 3900;
            break;

        case 8533:
        default:
            t.tRCD = 24;
            t.tRP = 24;
            t.tRAS = 56;
            t.tRC = 80;
            t.tCL = 22;
            t.tWL = 11;
            t.tWR = 45;
            t.tRTP = 16;
            t.tRRD_L = 11;
            t.tRRD_S = 5;
            t.tCCD_L = 11;
            t.tCCD_S = 5;
            t.tFAW = 43;
            t.tWTR_L = 22;
            t.tWTR_S = 11;
            t.tRTW = 24;
            t.tBurst = 8;
            t.tRFC = 280;
            t.tRFCpb = 90;
            t.tREFI = 3900;
            break;
    }

    return t;
}

// ============================================================================
// Cycle-Accurate Controller Implementation
// ============================================================================

CycleAccurateLPDDR5Controller::CycleAccurateLPDDR5Controller(const ControllerConfig& config)
    : config_(config)
    , banks_(config.organization.num_channels * config.organization.banks_per_rank())
{
    // Initialize scheduler
    SchedulerConfig sched_config;
    sched_config.policy = SchedulerPolicy::FR_FCFS;
    sched_config.buffer_size = config.queue_depth;
    sched_config.num_banks = static_cast<uint8_t>(banks_.size());

    scheduler_ = std::make_unique<FrFcfsScheduler>(sched_config);

    // Initialize refresh manager
    RefreshConfig ref_config;
    ref_config.policy = RefreshPolicy::PER_BANK;
    ref_config.tREFI = config.timing.tREFI;
    ref_config.tRFCpb = config.timing.tRFCpb;
    ref_config.num_banks = static_cast<uint8_t>(banks_.size());

    // refresh_ = create_refresh_manager(ref_config);  // TODO: implement
}

std::optional<RequestId> CycleAccurateLPDDR5Controller::submit(Request request) {
    if (!scheduler_->has_space()) {
        return std::nullopt;
    }

    request.id = next_id_++;
    request.submit_cycle = current_cycle_;
    decode_address(request);

    scheduler_->store(request);
    return request.id;
}

bool CycleAccurateLPDDR5Controller::can_accept() const {
    return scheduler_->has_space();
}

bool CycleAccurateLPDDR5Controller::has_pending() const {
    return scheduler_->has_any_pending();
}

size_t CycleAccurateLPDDR5Controller::pending_count() const {
    return scheduler_->occupancy();
}

void CycleAccurateLPDDR5Controller::tick() {
    current_cycle_++;

    // 1. Update bank state machines
    update_bank_states();

    // 2. Issue new commands
    issue_commands();

    // 3. Complete transfers
    complete_transfers();

    // 4. Check invariants
    if (check_invariants_) {
        check_timing_invariants();
    }
}

void CycleAccurateLPDDR5Controller::drain() {
    while (scheduler_->has_any_pending()) {
        tick();
    }
}

void CycleAccurateLPDDR5Controller::reset() {
    current_cycle_ = 0;
    next_id_ = 1;
    for (auto& bank : banks_) {
        bank = LPDDR5Bank{};
    }
    stats_.reset();
    violations_.clear();
}

BankState CycleAccurateLPDDR5Controller::bank_state(Channel channel, Bank bank) const {
    size_t idx = channel * config_.organization.banks_per_rank() + bank;
    if (idx < banks_.size()) {
        return banks_[idx].state;
    }
    return BankState::IDLE;
}

bool CycleAccurateLPDDR5Controller::is_row_open(Channel channel, Bank bank, Row row) const {
    size_t idx = channel * config_.organization.banks_per_rank() + bank;
    if (idx < banks_.size()) {
        return banks_[idx].state == BankState::ACTIVE && banks_[idx].open_row == row;
    }
    return false;
}

std::optional<Row> CycleAccurateLPDDR5Controller::open_row(Channel channel, Bank bank) const {
    size_t idx = channel * config_.organization.banks_per_rank() + bank;
    if (idx < banks_.size() && banks_[idx].state == BankState::ACTIVE) {
        return banks_[idx].open_row;
    }
    return std::nullopt;
}

void CycleAccurateLPDDR5Controller::decode_address(Request& request) {
    // Simple address decoding (row:bank:column)
    const auto& org = config_.organization;
    uint64_t addr = request.address;

    // Extract column bits
    request.column = static_cast<Column>(addr & ((1 << 10) - 1));
    addr >>= 10;

    // Extract bank bits
    request.bank = static_cast<Bank>(addr & (org.banks_per_rank() - 1));
    addr >>= 4;

    // Extract row bits
    request.row = static_cast<Row>(addr & ((1 << 16) - 1));
    addr >>= 16;

    // Extract channel
    request.channel = static_cast<Channel>(addr & (org.num_channels - 1));
}

void CycleAccurateLPDDR5Controller::update_bank_states() {
    for (auto& bank : banks_) {
        if (current_cycle_ >= bank.state_until) {
            switch (bank.state) {
                case BankState::ACTIVATING:
                    bank.state = BankState::ACTIVE;
                    break;
                case BankState::PRECHARGING:
                    bank.state = BankState::IDLE;
                    bank.open_row = 0;
                    break;
                case BankState::READING:
                case BankState::WRITING:
                    bank.state = BankState::ACTIVE;
                    break;
                case BankState::REFRESHING:
                    bank.state = BankState::IDLE;
                    break;
                default:
                    break;
            }
        }
    }
}

void CycleAccurateLPDDR5Controller::issue_commands() {
    // Simple round-robin across banks
    for (size_t i = 0; i < banks_.size(); ++i) {
        auto& bank = banks_[i];
        auto bank_idx = static_cast<Bank>(i);

        std::optional<Row> row_opt = (bank.state == BankState::ACTIVE)
            ? std::optional<Row>(bank.open_row)
            : std::nullopt;

        Request* req = scheduler_->get_next(bank_idx, row_opt, last_command_);
        if (!req) continue;

        // Check if we can issue the command
        if (bank.state == BankState::IDLE) {
            // Need to activate
            if (current_cycle_ >= bank.next_act) {
                bank.state = BankState::ACTIVATING;
                bank.open_row = req->row;
                bank.state_until = current_cycle_ + config_.timing.tRCD;
                bank.next_act = current_cycle_ + config_.timing.tRC;
                bank.next_rd = current_cycle_ + config_.timing.tRCD;
                bank.next_wr = current_cycle_ + config_.timing.tRCD;
            }
        } else if (bank.state == BankState::ACTIVE) {
            if (bank.open_row == req->row) {
                // Row hit
                if (bank.is_ready_for(req->type, current_cycle_)) {
                    stats_.page_hits++;

                    if (req->type == RequestType::READ) {
                        bank.state = BankState::READING;
                        bank.state_until = current_cycle_ + config_.timing.tBurst;
                        bank.next_rd = current_cycle_ + config_.timing.tCCD_S;
                        bank.next_wr = current_cycle_ + config_.timing.tRTW;
                        last_read_cycle_ = current_cycle_;
                    } else {
                        bank.state = BankState::WRITING;
                        bank.state_until = current_cycle_ + config_.timing.tBurst;
                        bank.next_wr = current_cycle_ + config_.timing.tCCD_S;
                        bank.next_rd = current_cycle_ + config_.timing.tWTR_S;
                        last_write_cycle_ = current_cycle_;
                    }

                    last_command_ = req->type;

                    // Record completion
                    Cycle latency = current_cycle_ - req->submit_cycle + config_.timing.tBurst;
                    stats_.record_request(req->type, latency, true, false);

                    if (req->callback) {
                        req->callback(latency);
                    }

                    scheduler_->remove(*req);
                }
            } else {
                // Row conflict - need to precharge first
                stats_.page_conflicts++;
                if (current_cycle_ >= bank.next_pre) {
                    bank.state = BankState::PRECHARGING;
                    bank.state_until = current_cycle_ + config_.timing.tRP;
                    bank.next_act = current_cycle_ + config_.timing.tRP;
                }
            }
        }
    }
}

void CycleAccurateLPDDR5Controller::complete_transfers() {
    // Transfers complete via callbacks in issue_commands
}

void CycleAccurateLPDDR5Controller::check_timing_invariants() {
    // TODO: Add timing invariant checks
}

} // namespace sw::memsim::lpddr5
