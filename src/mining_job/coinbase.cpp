// src/mining_job/coinbase.cpp
#include <stdexcept>

#include "mining_job/coinbase.hpp"

namespace cpu_miner {
namespace {

bool is_lower_hex_digit(char ch) {
   return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

bool is_hex_string(const std::string& s) {
   if ((s.size() % 2) != 0) return false;
   for (char ch : s) {
      if (!is_lower_hex_digit(ch)) return false;
   }
   return true;
}

} // namespace

std::string make_coinbase_hex(const MiningJob& job,
                              const SubscriptionContext& sub,
                              const std::string& extranonce2_hex) {
   if (!is_hex_string(job.coinb1)) {
      throw std::invalid_argument("coinb1 is not valid lowercase hex");
   }
   if (!is_hex_string(job.coinb2)) {
      throw std::invalid_argument("coinb2 is not valid lowercase hex");
   }
   if (!is_hex_string(sub.extranonce1)) {
      throw std::invalid_argument("extranonce1 is not valid lowercase hex");
   }
   if (!is_hex_string(extranonce2_hex)) {
      throw std::invalid_argument("extranonce2 is not valid lowercase hex");
   }

   const std::size_t expected_extranonce2_hex_chars = sub.extranonce2_size * 2;
   if (extranonce2_hex.size() != expected_extranonce2_hex_chars) {
      throw std::invalid_argument("extranonce2 has wrong hex length");
   }

   return job.coinb1 + sub.extranonce1 + extranonce2_hex + job.coinb2;
}

} // namespace cpu_miner

