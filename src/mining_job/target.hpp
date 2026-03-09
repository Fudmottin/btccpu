// src/mining_job/target.hpp

#ifndef CPU_MINER_MINING_JOB_TARGET_HPP
#define CPU_MINER_MINING_JOB_TARGET_HPP

#include "util/uint256.hpp"

namespace cpu_miner {
   using u256::uint256;

   // Expand Bitcoin compact target format to uint256.
   uint256 expand_compact_target(std::uint32_t compact);
}

#endif
