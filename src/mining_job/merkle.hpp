// src/mining_job/merkle.hpp

#ifndef CPU_MINER_MINING_JOB_MERKLE_HPP
#define CPU_MINER_MINING_JOB_MERKLE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cpu_miner {

using HashBytes = std::array<std::uint8_t, 32>;

HashBytes merkle_fold(HashBytes current_hash,
                      const std::vector<std::string>& merkle_branch);

std::string merkle_root_raw_hex(HashBytes coinbase_hash,
                            const std::vector<std::string>& merkle_branch);

} // namespace cpu_miner

#endif

