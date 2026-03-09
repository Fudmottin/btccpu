// src/mining_job/coinbase.cpp

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "mining_job/coinbase.hpp"
#include "util/hex.hpp"

namespace cpu_miner {

std::string extranonce2_from_counter(std::uint64_t value,
                                     std::size_t size_bytes) {
   return hex_from_u64_be(value, size_bytes);
}

std::string make_coinbase_hex(const MiningJob& job,
                              const SubscriptionContext& sub,
                              const std::string& extranonce2_hex) {
   if (!is_lower_hex_string(job.coinb1)) {
      throw std::invalid_argument("coinb1 is not valid lowercase hex");
   }
   if (!is_lower_hex_string(job.coinb2)) {
      throw std::invalid_argument("coinb2 is not valid lowercase hex");
   }
   if (!is_lower_hex_string(sub.extranonce1)) {
      throw std::invalid_argument("extranonce1 is not valid lowercase hex");
   }
   if (!is_lower_hex_string(extranonce2_hex)) {
      throw std::invalid_argument("extranonce2 is not valid lowercase hex");
   }

   const std::size_t expected_extranonce2_hex_chars = sub.extranonce2_size * 2U;
   if (extranonce2_hex.size() != expected_extranonce2_hex_chars) {
      throw std::invalid_argument("extranonce2 has wrong hex length");
   }

   std::string coinbase;
   coinbase.reserve(job.coinb1.size() + sub.extranonce1.size() +
                    extranonce2_hex.size() + job.coinb2.size());

   coinbase += job.coinb1;
   coinbase += sub.extranonce1;
   coinbase += extranonce2_hex;
   coinbase += job.coinb2;

   return coinbase;
}

} // namespace cpu_miner

