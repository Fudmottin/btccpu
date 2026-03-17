// src/mining_job/work_state.hpp

#ifndef CPU_MINER_MINING_JOB_WORK_STATE_HPP
#define CPU_MINER_MINING_JOB_WORK_STATE_HPP

#include <cstdint>
#include <string>

#include "mining_job/coinbase.hpp"
#include "mining_job/header.hpp"
#include "mining_job/job.hpp"
#include "mining_job/share.hpp"

namespace cpu_miner {

struct WorkState {
   MiningJob job;
   SubscriptionContext subscription;

   std::uint64_t extranonce2_counter{};
   std::uint32_t nonce{};

   CoinbaseBuild coinbase;
   std::string merkle_root_hex;

   HashBytes prevhash{};
   HashBytes merkle_root{};
   HeaderTemplate header_template{};

   [[nodiscard]] bool empty() const noexcept;
};

[[nodiscard]] WorkState make_work_state(const MiningJob& job,
                                        const SubscriptionContext& subscription,
                                        std::uint64_t extranonce2_counter = 0);

void reset_nonce(WorkState& work) noexcept;
void advance_extranonce2(WorkState& work);

[[nodiscard]] WorkState with_nonce(const WorkState& work, std::uint32_t nonce);

[[nodiscard]] ShareSubmission make_share_submission(const WorkState& work);

} // namespace cpu_miner

#endif

