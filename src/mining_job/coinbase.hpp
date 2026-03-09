// src/mining_job/coinbase.hpp
#ifndef CPU_MINER_MINING_JOB_COINBASE_HPP
#define CPU_MINER_MINING_JOB_COINBASE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "mining_job/job.hpp"

namespace cpu_miner {

std::string extranonce2_from_counter(std::uint64_t value,
                                     std::size_t size_bytes);

std::string make_coinbase_hex(const MiningJob& job,
                              const SubscriptionContext& sub,
                              const std::string& extranonce2_hex);

} // namespace cpu_miner

#endif

