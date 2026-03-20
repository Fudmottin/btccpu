// src/mining_job/work_state.cpp

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

#include "mining_job/merkle.hpp"
#include "mining_job/share.hpp"
#include "mining_job/work_state.hpp"
#include "util/hex.hpp"

namespace cpu_miner {
namespace {

std::array<std::uint8_t, 32> hash32_from_hex(std::string_view hex) {
   const auto bytes = hex_to_bytes(hex);
   if (bytes.size() != 32U) {
      throw std::invalid_argument("expected 32-byte hash hex");
   }

   std::array<std::uint8_t, 32> out{};
   for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = bytes[i];
   }
   return out;
}

void swap_bytes_in_each_u32(std::span<std::uint8_t, 32> bytes) {
   for (std::size_t i = 0; i < bytes.size(); i += 4U) {
      std::swap(bytes[i + 0], bytes[i + 3]);
      std::swap(bytes[i + 1], bytes[i + 2]);
   }
}

std::uint64_t max_extranonce2_value(std::size_t size_bytes) {
   if (size_bytes > 8U) {
      throw std::invalid_argument(
         "extranonce2 sizes above 8 bytes are not supported");
   }

   if (size_bytes == 0U) return 0U;
   if (size_bytes == 8U) return std::numeric_limits<std::uint64_t>::max();

   return (std::uint64_t{1} << (size_bytes * 8U)) - 1U;
}

void rebuild_derived_state(WorkState& work) {
   const auto max_value =
      max_extranonce2_value(work.subscription.extranonce2_size);
   if (work.extranonce2_counter > max_value) {
      throw std::overflow_error("extranonce2 counter exceeds configured size");
   }

   const std::string extranonce2 =
      extranonce2_from_counter(work.extranonce2_counter,
                               work.subscription.extranonce2_size);

   work.coinbase = build_coinbase(work.job, work.subscription, extranonce2);

   work.merkle_root_raw_hex =
      merkle_root_raw_hex(work.coinbase.coinbase_hash, work.job.merkle_branch);

   work.prevhash = hash32_from_hex(work.job.prevhash);
   work.merkle_root = hash32_from_hex(work.merkle_root_raw_hex);

   swap_bytes_in_each_u32(std::span<std::uint8_t, 32>(work.prevhash));

   const std::uint32_t version = u32_from_hex_be(work.job.version);
   const std::uint32_t ntime = u32_from_hex_be(work.job.ntime);
   const std::uint32_t nbits = u32_from_hex_be(work.job.nbits);

   work.header_template =
      make_header_template(version, work.prevhash, work.merkle_root, ntime,
                           nbits, work.nonce);
}

} // namespace

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
   WorkState work;
   work.job = job;
   work.subscription = subscription;
   work.extranonce2_counter = extranonce2_counter;
   work.nonce = 0U;

   rebuild_derived_state(work);
   reset_nonce(work);
   return work;
}

void reset_nonce(WorkState& work) noexcept {
   work.nonce = 0U;
   set_header_nonce(work.header_template, work.nonce);
}

void advance_extranonce2(WorkState& work) {
   const auto max_value =
      max_extranonce2_value(work.subscription.extranonce2_size);
   if (work.extranonce2_counter >= max_value) {
      throw std::overflow_error("extranonce2 counter overflow");
   }

   ++work.extranonce2_counter;
   work.nonce = 0U;
   rebuild_derived_state(work);
   reset_nonce(work);
}

WorkState with_nonce(const WorkState& work, std::uint32_t nonce) {
   WorkState copy = work;
   copy.nonce = nonce;
   set_header_nonce(copy.header_template, nonce);
   return copy;
}

} // namespace cpu_miner

