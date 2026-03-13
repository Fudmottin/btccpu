// src/mining_job/scan.cpp

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "mining_job/header.hpp"
#include "mining_job/scan.hpp"
#include "mining_job/target.hpp"
#include "sha256/sha256.hpp"
#include "stratum_client/stratum_client.hpp"
#include "util/hex.hpp"

namespace cpu_miner {
namespace {

std::string short_hash_hex(const sha256::DigestBytes& digest_bytes,
                           std::size_t chars = 16U) {
   const std::string hex = bytes_to_hex(digest_bytes);
   return hex.substr(0, std::min(chars, hex.size()));
}

void print_scan_line(std::uint32_t nonce,
                     const sha256::DigestBytes& digest_bytes,
                     bool meets_network, bool meets_share) {
   std::cout << "  nonce=" << std::setw(10) << nonce
             << " hash=" << short_hash_hex(digest_bytes)
             << " net=" << (meets_network ? 'Y' : 'n')
             << " share=" << (meets_share ? 'Y' : 'n') << '\n';
}

void print_progress(std::uint64_t hashes_done, std::uint64_t total_hashes,
                    const std::chrono::steady_clock::time_point& start_time) {
   static constexpr std::array<char, 4> spinner{'-', '\\', '|', '/'};
   const char spin = spinner[static_cast<std::size_t>(
      (hashes_done / 1000000ULL) % spinner.size())];

   const auto now = std::chrono::steady_clock::now();
   const std::chrono::duration<double> elapsed = now - start_time;
   const double seconds = elapsed.count();
   const double rate =
      (seconds > 0.0) ? static_cast<double>(hashes_done) / seconds : 0.0;
   const double percent = (total_hashes > 0U)
                             ? 100.0 * static_cast<double>(hashes_done) /
                                  static_cast<double>(total_hashes)
                             : 0.0;

   std::cout << '\r' << spin << " progress: " << hashes_done << '/'
             << total_hashes << " (" << std::fixed << std::setprecision(2)
             << percent << "%)"
             << " rate: " << std::setprecision(0) << rate << " H/s"
             << std::flush;
}

} // namespace

ScanResult scan_nonce_range(StratumClient& client, WorkState& work,
                            const uint256& network_target,
                            const uint256& share_target,
                            std::uint64_t nonce_begin, std::uint64_t nonce_end,
                            std::uint64_t progress_interval) {
   if (nonce_begin > nonce_end) {
      throw std::invalid_argument("scan_nonce_range: nonce_begin > nonce_end");
   }

   if (nonce_end >
       static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
      throw std::out_of_range("scan_nonce_range: nonce_end exceeds uint32_t");
   }

   ScanResult result{};

   const auto start_time = std::chrono::steady_clock::now();
   const std::uint64_t total_hashes = (nonce_end - nonce_begin) + 1ULL;

   for (std::uint64_t scan_nonce = nonce_begin; scan_nonce <= nonce_end;
        ++scan_nonce) {
      const auto nonce = static_cast<std::uint32_t>(scan_nonce);
      work.nonce = nonce;

      set_header_nonce(work.header_template, nonce);

      const auto hash_words = hash_header_template(work.header_template);
      const auto hash_bytes = sha256::digest_words_to_bytes_be(hash_words);

      ++result.hashes_done;

      if (progress_interval > 0U &&
          (result.hashes_done % progress_interval) == 0ULL) {
         print_progress(result.hashes_done, total_hashes, start_time);
      }

      const bool meets_network = hash_meets_target(hash_bytes, network_target);
      const bool meets_share = hash_meets_target(hash_bytes, share_target);

      const bool should_print =
         ((nonce_begin == 0ULL) && (scan_nonce < 8ULL)) || meets_network ||
         meets_share || (scan_nonce == nonce_end);

      if (should_print) {
         if (progress_interval > 0U &&
             result.hashes_done >= progress_interval) {
            std::cout << '\n';
         }
         print_scan_line(nonce, hash_bytes, meets_network, meets_share);
      }

      if (meets_network) {
         ++result.blocks_found;
         std::cout << "  BLOCK CANDIDATE nonce=" << nonce
                   << " hash=" << bytes_to_hex(hash_bytes) << '\n';
      }

      if (meets_share) {
         ++result.shares_found;
         if (progress_interval > 0U && result.hashes_done >= progress_interval) {
            std::cout << '\n';
         }
         std::cout << "  SHARE HIT nonce=" << nonce
                   << " hash=" << bytes_to_hex(hash_bytes) << '\n';

         const auto submission = make_share_submission(work);
         const bool accepted = client.submit_share(submission);

         if (accepted) {
            ++result.shares_accepted;
         } else {
            ++result.shares_rejected;
         }

         std::cout << "  share submission: "
                   << (accepted ? "accepted" : "rejected") << '\n';
      }
   }

   if (progress_interval > 0U && result.hashes_done >= progress_interval) {
      std::cout << '\n';
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

