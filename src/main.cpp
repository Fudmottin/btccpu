// src/main.cpp

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "mining_job/coinbase.hpp"
#include "mining_job/header.hpp"
#include "mining_job/target.hpp"
#include "mining_job/work_state.hpp"
#include "sha256/sha256.hpp"
#include "stratum_client/stratum_client.hpp"
#include "util/hex.hpp"

namespace {

std::uint64_t require_integral_difficulty(double difficulty) {
   if (!(difficulty > 0.0) || !std::isfinite(difficulty)) {
      throw std::invalid_argument("stratum difficulty must be finite and > 0");
   }

   const auto as_u64 = static_cast<std::uint64_t>(difficulty);
   if (static_cast<double>(as_u64) != difficulty) {
      throw std::runtime_error(
         "stratum difficulty is not an exact integer; integer share-target "
         "path requested");
   }

   return as_u64;
}

std::string short_hash_hex(const cpu_miner::sha256::DigestBytes& digest_bytes,
                           std::size_t chars = 16U) {
   const std::string hex = cpu_miner::bytes_to_hex(digest_bytes);
   return hex.substr(0, std::min(chars, hex.size()));
}

void print_scan_line(std::uint32_t nonce,
                     const cpu_miner::sha256::DigestBytes& digest_bytes,
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

int main(int argc, char* argv[]) {
   try {
      const std::string host = (argc > 1) ? argv[1] : "192.168.0.104";
      const std::string port = (argc > 2) ? argv[2] : "3333";
      const std::string user =
         (argc > 3) ? argv[3] : "bc1qyourwalletaddresshere.cpu-miner";
      const std::string password = (argc > 4) ? argv[4] : "x";

      cpu_miner::StratumClient client(host, port);
      client.connect();
      client.subscribe();
      client.suggest_difficulty(1.0);
      client.authorize(user, password);
      client.run_until_notify();

      if (client.subscription() && client.current_job()) {
         std::cout << "Difficulty: " << client.difficulty() << "\n";

         const auto& sub = *client.subscription();
         const auto& job = *client.current_job();
         auto work = cpu_miner::make_work_state(job, sub, 0U);

         std::cout << "coinbase demo:\n";
         std::cout << "  extranonce2: " << work.coinbase.extranonce2_hex
                   << '\n';
         std::cout << "  coinbase hex chars: "
                   << work.coinbase.coinbase_hex.size() << '\n';
         std::cout << "  coinbase bytes: "
                   << work.coinbase.coinbase_bytes.size() << '\n';

         const std::size_t preview_len =
            std::min<std::size_t>(work.coinbase.coinbase_hex.size(), 64U);
         std::cout << "  coinbase prefix: "
                   << work.coinbase.coinbase_hex.substr(0, preview_len) << '\n';
         std::cout << "  coinbase suffix: "
                   << work.coinbase.coinbase_hex.substr(
                         work.coinbase.coinbase_hex.size() > 192
                            ? work.coinbase.coinbase_hex.size() - 192
                            : 0)
                   << '\n';

         const std::string coinbase_hash_hex =
            cpu_miner::bytes_to_hex(work.coinbase.coinbase_hash);
         std::cout << "  coinbase_hash: " << coinbase_hash_hex << '\n';

         const auto decoded_coinbase =
            cpu_miner::decode_coinbase(work.coinbase.coinbase_bytes);
         cpu_miner::print_coinbase_decoded(decoded_coinbase, std::cout);

         std::cout << "  merkle_root: " << work.merkle_root_hex << '\n';

         const std::uint32_t version = cpu_miner::u32_from_hex_be(job.version);
         const std::uint32_t ntime = cpu_miner::u32_from_hex_be(job.ntime);
         const std::uint32_t nbits = cpu_miner::u32_from_hex_be(job.nbits);
         const std::uint32_t nonce0 = 0U;

         const auto header =
            cpu_miner::make_header_bytes(version, work.prevhash,
                                         work.merkle_root, ntime, nbits,
                                         nonce0);

         const std::string header_hex = cpu_miner::header_hex(header);
         const auto header_hash_words =
            cpu_miner::sha256::dbl_sha256_words(header);
         const auto header_hash_bytes =
            cpu_miner::sha256::digest_words_to_bytes_be(header_hash_words);
         const std::string header_hash_hex =
            cpu_miner::bytes_to_hex(header_hash_bytes);

         std::cout << "  header_hex: " << header_hex << '\n';
         std::cout << "  header_hash_nonce0: " << header_hash_hex << '\n';

         cpu_miner::set_header_nonce(work.header_template, nonce0);

         const auto template_hash_words =
            cpu_miner::hash_header_template(work.header_template);
         const auto template_hash_bytes =
            cpu_miner::sha256::digest_words_to_bytes_be(template_hash_words);
         const std::string template_hash_hex =
            cpu_miner::bytes_to_hex(template_hash_bytes);

         std::cout << "  template_hash_nonce0: " << template_hash_hex << '\n';

         const auto network_target = cpu_miner::expand_compact_target(nbits);
         const auto share_difficulty =
            require_integral_difficulty(client.difficulty());
         const auto share_target =
            cpu_miner::share_target_from_difficulty(share_difficulty);

         std::cout << "nonce scan:\n";
         std::cout << "  nbits: 0x" << std::hex << std::setw(8)
                   << std::setfill('0') << nbits << std::dec
                   << std::setfill(' ') << '\n';
         std::cout << "  share difficulty (integer): " << share_difficulty
                   << '\n';

         constexpr std::uint64_t kNonceBegin = 0ULL;
         constexpr std::uint64_t kNonceEnd = 20'000'000ULL;
         constexpr std::uint64_t kProgressInterval = 1'000'000ULL;

         bool found_share = false;
         bool found_block = false;

         cpu_miner::reset_nonce(work);

         const auto start_time = std::chrono::steady_clock::now();
         const std::uint64_t total_hashes = (kNonceEnd - kNonceBegin) + 1ULL;
         std::uint64_t hashes_done = 0ULL;

         for (std::uint64_t scan_nonce = kNonceBegin; scan_nonce <= kNonceEnd;
              ++scan_nonce) {
            const auto nonce = static_cast<std::uint32_t>(scan_nonce);
            work.nonce = nonce;

            cpu_miner::set_header_nonce(work.header_template, nonce);

            const auto hash_words =
               cpu_miner::hash_header_template(work.header_template);
            const auto hash_bytes =
               cpu_miner::sha256::digest_words_to_bytes_be(hash_words);

            ++hashes_done;

            if ((hashes_done % kProgressInterval) == 0ULL) {
               print_progress(hashes_done, total_hashes, start_time);
            }

            const bool meets_network =
               cpu_miner::hash_meets_target(hash_bytes, network_target);
            const bool meets_share =
               cpu_miner::hash_meets_target(hash_bytes, share_target);

            const bool should_print = (scan_nonce < 8ULL) || meets_network ||
                                      meets_share || (scan_nonce == kNonceEnd);

            if (should_print) {
               if (hashes_done >= kProgressInterval) {
                  std::cout << '\n';
               }
               print_scan_line(nonce, hash_bytes, meets_network, meets_share);
            }

            if (meets_network) {
               found_block = true;
               std::cout << "  BLOCK CANDIDATE nonce=" << nonce
                         << " hash=" << cpu_miner::bytes_to_hex(hash_bytes)
                         << '\n';
            }

            if (meets_share) {
               found_share = true;
               std::cout << "  SHARE HIT nonce=" << nonce
                         << " hash=" << cpu_miner::bytes_to_hex(hash_bytes)
                         << '\n';

               const auto submission = cpu_miner::make_share_submission(work);
               const bool accepted = client.submit_share(submission);
               std::cout << "  share submission: "
                         << (accepted ? "accepted" : "rejected") << '\n';
            }
         }

         if (hashes_done >= kProgressInterval) {
            std::cout << '\n';
         }

         const auto end_time = std::chrono::steady_clock::now();
         const std::chrono::duration<double> elapsed = end_time - start_time;
         const double rate =
            (elapsed.count() > 0.0)
               ? static_cast<double>(hashes_done) / elapsed.count()
               : 0.0;

         std::cout << "  scanned hashes: " << hashes_done << '\n';
         std::cout << "  elapsed seconds: " << std::fixed
                   << std::setprecision(3) << elapsed.count() << '\n';
         std::cout << "  average rate: " << std::setprecision(0) << rate
                   << " H/s\n";

         if (!found_share) {
            std::cout << "  no share hit in nonce range [" << kNonceBegin
                      << ", " << kNonceEnd << "]\n";
         }

         if (!found_block) {
            std::cout << "  no block candidate in nonce range [" << kNonceBegin
                      << ", " << kNonceEnd << "]\n";
         }
      } else {
         std::cout << "coinbase demo skipped: missing subscription or job\n";
      }

      std::cout << "Probe complete\n";
      return 0;
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

