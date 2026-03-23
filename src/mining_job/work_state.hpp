// src/mining_job/work_state.hpp

#ifndef CPU_MINER_MINING_JOB_WORK_STATE_HPP
#define CPU_MINER_MINING_JOB_WORK_STATE_HPP

#include <cstdint>
#include <string>
#include <string_view>

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
   std::string merkle_root_raw_hex;

   HashBytes prevhash_sha_input{};
   HashBytes merkle_root_sha_input{};
   HeaderTemplate header_template{};

   [[nodiscard]] bool empty() const noexcept;
};

[[nodiscard]] std::string
compute_merkle_root_raw_hex(const CoinbaseBuild& coinbase,
                            const MiningJob& job);

[[nodiscard]] HashBytes prevhash_sha_input_from_job(const MiningJob& job);

[[nodiscard]] HashBytes
merkle_root_sha_input_from_hex(std::string_view merkle_root_raw_hex);

[[nodiscard]] HeaderTemplate make_work_header_template(
   const MiningJob& job, const HashBytes& prevhash_sha_input,
   const HashBytes& merkle_root_sha_input, std::uint32_t nonce);

[[nodiscard]] WorkState make_work_state(const MiningJob& job,
                                        const SubscriptionContext& subscription,
                                        std::uint64_t extranonce2_counter = 0);

void reset_nonce(WorkState& work) noexcept;
void advance_extranonce2(WorkState& work);

[[nodiscard]] WorkState with_nonce(const WorkState& work, std::uint32_t nonce);

[[nodiscard]] ShareSubmission make_share_submission(const WorkState& work);

} // namespace cpu_miner

#endif
