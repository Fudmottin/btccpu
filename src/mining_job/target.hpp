// src/mining_job/target.hpp

#ifndef CPU_MINER_MINING_JOB_TARGET_HPP
#define CPU_MINER_MINING_JOB_TARGET_HPP

#include <cstdint>

#include "sha256/sha256.hpp"
#include "util/uint256.hpp"

namespace cpu_miner {

using u256::uint256;

// Expand Bitcoin compact target format to uint256.
uint256 expand_compact_target(std::uint32_t compact);

// Bitcoin "difficulty 1" target.
uint256 difficulty_1_target();

// Pool share target for a given Stratum difficulty.
uint256 share_target_from_difficulty(double difficulty);
uint256 share_target_from_difficulty(std::uint64_t difficulty);

// Interpret SHA-256 digest bytes as a Bitcoin hash integer for target compare.
uint256 hash_digest_to_uint256(const sha256::DigestBytes& digest_bytes);

// True iff hash <= target.
bool hash_meets_target(const sha256::DigestBytes& digest_bytes,
                       const uint256& target);

} // namespace cpu_miner

#endif

