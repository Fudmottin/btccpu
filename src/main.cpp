// src/main.cpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "mining_job/coinbase.hpp"
#include "mining_job/header.hpp"
#include "mining_job/scan.hpp"
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

void print_startup_sanity(const cpu_miner::WorkState& work) {
   std::cout << "coinbase demo:\n";
   std::cout << "  extranonce2: " << work.coinbase.extranonce2_hex << '\n';
   std::cout << "  coinbase hex chars: " << work.coinbase.coinbase_hex.size()
             << '\n';
   std::cout << "  coinbase bytes: " << work.coinbase.coinbase_bytes.size()
             << '\n';

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

   const std::uint32_t version = cpu_miner::u32_from_hex_be(work.job.version);
   const std::uint32_t ntime = cpu_miner::u32_from_hex_be(work.job.ntime);
   const std::uint32_t nbits = cpu_miner::u32_from_hex_be(work.job.nbits);
   const std::uint32_t nonce0 = 0U;

   const auto header =
      cpu_miner::make_header_bytes(version, work.prevhash, work.merkle_root,
                                   ntime, nbits, nonce0);

   const std::string header_hex = cpu_miner::header_hex(header);
   const auto header_hash_words = cpu_miner::sha256::dbl_sha256_words(header);
   const auto header_hash_bytes =
      cpu_miner::sha256::digest_words_to_bytes_be(header_hash_words);
   const std::string header_hash_hex =
      cpu_miner::bytes_to_hex(header_hash_bytes);

   std::cout << "  header_hex: " << header_hex << '\n';
   std::cout << "  header_hash_nonce0: " << header_hash_hex << '\n';

   auto template_copy = work.header_template;
   cpu_miner::set_header_nonce(template_copy, nonce0);

   const auto template_hash_words =
      cpu_miner::hash_header_template(template_copy);
   const auto template_hash_bytes =
      cpu_miner::sha256::digest_words_to_bytes_be(template_hash_words);
   const std::string template_hash_hex =
      cpu_miner::bytes_to_hex(template_hash_bytes);

   std::cout << "  template_hash_nonce0: " << template_hash_hex << '\n';
}

void print_chunk_summary(const cpu_miner::ScanResult& result) {
   std::cout << "  scanned hashes: " << result.hashes_done << '\n';
   std::cout << "  shares found: " << result.shares_found << '\n';
   std::cout << "  blocks found: " << result.blocks_found << '\n';
   std::cout << "  shares accepted: " << result.shares_accepted << '\n';
   std::cout << "  shares rejected: " << result.shares_rejected << '\n';
   std::cout << "  elapsed seconds: " << std::fixed << std::setprecision(3)
             << result.elapsed_seconds << '\n';
   std::cout << "  average rate: " << std::setprecision(0)
             << result.hash_rate_hps << " H/s\n";
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

      if (!(client.subscription() && client.current_job())) {
         std::cout << "mining skipped: missing subscription or job\n";
         return 0;
      }

      std::cout << "Difficulty: " << client.difficulty() << "\n";

      const auto& sub = *client.subscription();
      const auto& job = *client.current_job();
      auto work = cpu_miner::make_work_state(job, sub, 0U);

      print_startup_sanity(work);

      const std::uint32_t nbits = cpu_miner::u32_from_hex_be(job.nbits);
      const auto network_target = cpu_miner::expand_compact_target(nbits);
      const auto share_difficulty =
         require_integral_difficulty(client.difficulty());
      const auto share_target =
         cpu_miner::share_target_from_difficulty(share_difficulty);

      std::cout << "mining setup:\n";
      std::cout << "  nbits: 0x" << std::hex << std::setw(8)
                << std::setfill('0') << nbits << std::dec << std::setfill(' ')
                << '\n';
      std::cout << "  share difficulty (integer): " << share_difficulty << '\n';

      constexpr std::uint64_t kNonceChunkSize =
      static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL;
      constexpr std::uint64_t kProgressInterval = 1'000'000ULL;
      constexpr std::uint64_t kMaxNonce =
      static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());

      std::uint64_t total_hashes_done = 0ULL;
      std::uint64_t total_shares_found = 0ULL;
      std::uint64_t total_blocks_found = 0ULL;
      std::uint64_t total_shares_accepted = 0ULL;
      std::uint64_t total_shares_rejected = 0ULL;

      cpu_miner::reset_nonce(work);

      while (true) {
         const std::uint64_t nonce_begin = work.nonce;
         const std::uint64_t remaining = (kMaxNonce - nonce_begin) + 1ULL;
         const std::uint64_t this_chunk_hashes =
            std::min(kNonceChunkSize, remaining);
         const std::uint64_t nonce_end = nonce_begin + this_chunk_hashes - 1ULL;

         std::cout << "mining chunk:\n";
         std::cout << "  job_id: " << work.job.job_id << '\n';
         std::cout << "  extranonce2: " << work.coinbase.extranonce2_hex
                   << '\n';
         std::cout << "  nonce range: [" << nonce_begin << ", " << nonce_end
                   << "]\n";

         const auto result =
            cpu_miner::scan_nonce_range(client, work, network_target,
                                        share_target, nonce_begin, nonce_end,
                                        kProgressInterval);

         total_hashes_done += result.hashes_done;
         total_shares_found += result.shares_found;
         total_blocks_found += result.blocks_found;
         total_shares_accepted += result.shares_accepted;
         total_shares_rejected += result.shares_rejected;

         print_chunk_summary(result);

         std::cout << "running totals:\n";
         std::cout << "  hashes: " << total_hashes_done << '\n';
         std::cout << "  shares found: " << total_shares_found << '\n';
         std::cout << "  blocks found: " << total_blocks_found << '\n';
         std::cout << "  shares accepted: " << total_shares_accepted << '\n';
         std::cout << "  shares rejected: " << total_shares_rejected << '\n';

         if (nonce_end == kMaxNonce) {
            std::cout << "nonce space exhausted; advancing extranonce2\n";
            cpu_miner::advance_extranonce2(work);
            continue;
         }

         work.nonce = static_cast<std::uint32_t>(nonce_end + 1ULL);
      }
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

