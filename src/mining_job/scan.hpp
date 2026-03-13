// src/mining_job/scan.hpp

#ifndef CPU_MINER_MINING_JOB_SCAN_HPP
#define CPU_MINER_MINING_JOB_SCAN_HPP

#include <cstdint>

#include "mining_job/work_state.hpp"
#include "util/uint256.hpp"

namespace cpu_miner {

class StratumClient;

struct ScanResult {
   std::uint64_t hashes_done{};
   std::uint64_t shares_found{};
   std::uint64_t blocks_found{};
   std::uint64_t shares_accepted{};
   std::uint64_t shares_rejected{};
   double elapsed_seconds{};
   double hash_rate_hps{};
};

[[nodiscard]] ScanResult scan_nonce_range(StratumClient& client,
                                          WorkState& work,
                                          const u256::uint256& network_target,
                                          const u256::uint256& share_target,
                                          std::uint64_t nonce_begin,
                                          std::uint64_t nonce_end,
                                          std::uint64_t progress_interval);

} // namespace cpu_miner

#endif

