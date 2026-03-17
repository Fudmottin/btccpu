// src/mining_job/scan.hpp

#ifndef CPU_MINER_MINING_JOB_SCAN_HPP
#define CPU_MINER_MINING_JOB_SCAN_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <stop_token>

#include "mining_job/work_state.hpp"
#include "util/uint256.hpp"

namespace cpu_miner {

enum class ScanStopReason {
   exhausted,
   stale,
   stop_requested,
};

struct ShareCandidate {
   WorkState work;
   std::uint32_t nonce{};
   sha256::DigestBytes hash{};
   bool is_block_candidate{};
   std::uint64_t generation{};
};

struct ScanControl {
   std::stop_token stop_token;
   const std::atomic<std::uint64_t>* work_generation{};
   std::uint64_t expected_generation{};
   std::uint64_t check_interval{16'384U};
   std::atomic<std::uint64_t>* progress_hashes_done{};
};

struct ScanResult {
   std::uint64_t hashes_done{};
   std::uint64_t shares_found{};
   std::uint64_t blocks_found{};
   double elapsed_seconds{};
   double hash_rate_hps{};
   ScanStopReason stop_reason{ScanStopReason::exhausted};
};

using ShareFoundCallback =
   std::function<void(std::uint32_t nonce, const sha256::DigestBytes& hash,
                      bool is_block_candidate)>;

[[nodiscard]] ScanResult
scan_nonce_range(WorkState& work, const u256::uint256& network_target,
                 const u256::uint256& share_target, std::uint64_t nonce_begin,
                 std::uint64_t nonce_end, std::uint64_t progress_interval,
                 const ScanControl& control, ShareFoundCallback on_share_found);

} // namespace cpu_miner

#endif

