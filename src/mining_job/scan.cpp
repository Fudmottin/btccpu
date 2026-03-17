// src/mining_job/scan.cpp

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "mining_job/header.hpp"
#include "mining_job/scan.hpp"
#include "mining_job/target.hpp"
#include "sha256/sha256.hpp"

namespace cpu_miner {
namespace {

[[nodiscard]] bool generation_changed(const ScanControl& control,
                                      std::uint64_t hashes_done) {
   if (control.work_generation == nullptr) return false;
   if (control.check_interval == 0U) return false;
   if ((hashes_done % control.check_interval) != 0U) return false;

   return control.work_generation->load(std::memory_order_acquire) !=
          control.expected_generation;
}

} // namespace

ScanResult scan_nonce_range(WorkState& work,
                            const u256::uint256& network_target,
                            const u256::uint256& share_target,
                            std::uint64_t nonce_begin, std::uint64_t nonce_end,
                            std::uint64_t /*progress_interval*/,
                            const ScanControl& control,
                            ShareFoundCallback on_share_found) {
   if (nonce_begin > nonce_end) {
      throw std::invalid_argument("scan_nonce_range: nonce_begin > nonce_end");
   }

   if (nonce_end >
       static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
      throw std::out_of_range("scan_nonce_range: nonce_end exceeds uint32_t");
   }

   ScanResult result{};

   if (control.progress_hashes_done != nullptr) {
      control.progress_hashes_done->store(0U, std::memory_order_relaxed);
   }

   const auto start_time = std::chrono::steady_clock::now();

   for (std::uint64_t scan_nonce = nonce_begin; scan_nonce <= nonce_end;
        ++scan_nonce) {
      if (control.stop_token.stop_requested()) {
         result.stop_reason = ScanStopReason::stop_requested;
         break;
      }

      const auto nonce = static_cast<std::uint32_t>(scan_nonce);
      work.nonce = nonce;

      set_header_nonce(work.header_template, nonce);

      const auto hash_words = hash_header_template(work.header_template);
      const auto hash_bytes = sha256::digest_words_to_bytes_be(hash_words);

      ++result.hashes_done;

      if (control.progress_hashes_done != nullptr &&
          (control.check_interval == 0U ||
           (result.hashes_done % control.check_interval) == 0U)) {
         control.progress_hashes_done->store(result.hashes_done,
                                             std::memory_order_relaxed);
      }

      if (generation_changed(control, result.hashes_done)) {
         result.stop_reason = ScanStopReason::stale;
         break;
      }

      const bool meets_network = hash_meets_target(hash_bytes, network_target);
      const bool meets_share = hash_meets_target(hash_bytes, share_target);

      if (meets_network) {
         ++result.blocks_found;
      }

      if (meets_share) {
         ++result.shares_found;
         if (on_share_found) {
            on_share_found(nonce, hash_bytes, meets_network);
         }
      }
   }

   if (control.progress_hashes_done != nullptr) {
      control.progress_hashes_done->store(result.hashes_done,
                                          std::memory_order_relaxed);
   }

   const auto end_time = std::chrono::steady_clock::now();
   const std::chrono::duration<double> elapsed = end_time - start_time;

   result.elapsed_seconds = elapsed.count();
   result.hash_rate_hps =
      (result.elapsed_seconds > 0.0)
         ? static_cast<double>(result.hashes_done) / result.elapsed_seconds
         : 0.0;

   return result;
}

} // namespace cpu_miner

