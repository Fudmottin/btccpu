// src/mining_job/work_state.cpp

#include "mining_job/work_state.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "mining_job/merkle.hpp"
#include "mining_job/share.hpp"
#include "util/endian.hpp"
#include "util/hex.hpp"

namespace cpu_miner {
namespace {

std::uint64_t max_extranonce2_value(std::size_t size_bytes) {
   if (size_bytes > 8U) {
      throw std::invalid_argument(
         "extranonce2 sizes above 8 bytes are not supported");
   }

   if (size_bytes == 0U) return 0U;
   if (size_bytes == 8U) return std::numeric_limits<std::uint64_t>::max();

   return (std::uint64_t{1} << (size_bytes * 8U)) - 1U;
}

} // namespace

std::string compute_merkle_root_raw_hex(const CoinbaseBuild& coinbase,
                                        const MiningJob& job) {
   return merkle_root_raw_hex(coinbase.coinbase_hash, job.merkle_branch);
}

HashBytes prevhash_sha_input_from_job(const MiningJob& job) {
   auto prevhash = hex_to_array_32(job.prevhash);
   cpu_miner::util::byteswap_each_u32(prevhash);
   return prevhash;
}

HashBytes merkle_root_sha_input_from_hex(std::string_view merkle_root_raw_hex) {
   return hex_to_array_32(merkle_root_raw_hex);
}

HeaderTemplate make_work_header_template(const MiningJob& job,
                                         const HashBytes& prevhash_sha_input,
                                         const HashBytes& merkle_root_sha_input,
                                         std::uint32_t nonce) {
   const std::uint32_t version = u32_from_hex_be(job.version);
   const std::uint32_t ntime = u32_from_hex_be(job.ntime);
   const std::uint32_t nbits = u32_from_hex_be(job.nbits);

   return make_sha_header_template(version, prevhash_sha_input,
                                   merkle_root_sha_input, ntime, nbits, nonce);
}

PreparedWork prepare_work(const MiningJob& job,
                          const SubscriptionContext& subscription,
                          std::uint64_t extranonce2_counter) {
   const auto max_value = max_extranonce2_value(subscription.extranonce2_size);
   if (extranonce2_counter > max_value) {
      throw std::overflow_error("extranonce2 counter exceeds configured size");
   }

   PreparedWork prepared;
   prepared.job = job;
   prepared.subscription = subscription;
   prepared.extranonce2_counter = extranonce2_counter;
   prepared.extranonce2_hex =
      extranonce2_from_counter(extranonce2_counter,
                               subscription.extranonce2_size);

   prepared.coinbase =
      build_coinbase(job, subscription, prepared.extranonce2_hex);
   prepared.merkle_root_raw_hex =
      compute_merkle_root_raw_hex(prepared.coinbase, job);
   prepared.prevhash_sha_input = prevhash_sha_input_from_job(job);
   prepared.merkle_root_sha_input =
      merkle_root_sha_input_from_hex(prepared.merkle_root_raw_hex);
   prepared.header_template =
      make_work_header_template(job, prepared.prevhash_sha_input,
                                prepared.merkle_root_sha_input, 0U);

   return prepared;
}

sha256::DigestBytes hash_prepared_work_nonce(const PreparedWork& prepared,
                                             std::uint32_t nonce) {
   HeaderTemplate header = prepared.header_template;
   set_header_nonce(header, nonce);
   const auto hash_words = hash_header_template(header);
   return sha256::digest_words_to_bytes_be(hash_words);
}

WorkState work_state_from_prepared(const PreparedWork& prepared,
                                   std::uint32_t nonce) {
   WorkState work;
   work.job = prepared.job;
   work.subscription = prepared.subscription;
   work.extranonce2_counter = prepared.extranonce2_counter;
   work.nonce = nonce;
   work.coinbase = prepared.coinbase;
   work.merkle_root_raw_hex = prepared.merkle_root_raw_hex;
   work.prevhash_sha_input = prepared.prevhash_sha_input;
   work.merkle_root_sha_input = prepared.merkle_root_sha_input;
   work.header_template = prepared.header_template;
   set_header_nonce(work.header_template, nonce);
   return work;
}

ShareSubmission make_share_submission(const WorkState& work) {
   return ShareSubmission{
      .job_id = work.job.job_id,
      .extranonce2_hex = work.coinbase.extranonce2_hex,
      .ntime_hex = work.job.ntime,
      .nonce_hex = hex_from_u32_be(work.nonce),
   };
}

bool WorkState::empty() const noexcept {
   return job.job_id.empty() && subscription.extranonce1.empty() &&
          coinbase.coinbase_bytes.empty();
}

WorkState make_work_state(const MiningJob& job,
                          const SubscriptionContext& subscription,
                          std::uint64_t extranonce2_counter) {
   return work_state_from_prepared(prepare_work(job, subscription,
                                                extranonce2_counter),
                                   0U);
}

void reset_nonce(WorkState& work) noexcept {
   work.nonce = 0U;
   set_header_nonce(work.header_template, work.nonce);
}

void advance_extranonce2(WorkState& work) {
   const auto next_counter = work.extranonce2_counter + 1U;
   const auto prepared =
      prepare_work(work.job, work.subscription, next_counter);
   work = work_state_from_prepared(prepared, 0U);
}

WorkState with_nonce(const WorkState& work, std::uint32_t nonce) {
   WorkState copy = work;
   copy.nonce = nonce;
   set_header_nonce(copy.header_template, nonce);
   return copy;
}

} // namespace cpu_miner
