// src/mining_job/target.cpp

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "mining_job/target.hpp"

namespace cpu_miner {

using u256::uint256;

// Expand Bitcoin compact target format to uint256.
uint256 expand_compact_target(std::uint32_t compact) {
   const std::uint32_t exponent = (compact >> 24U) & 0xffU;
   const std::uint32_t coefficient = compact & 0x007fffffU;

   uint256 target{};
   if (exponent <= 3U) {
      target = uint256{coefficient >> (8U * (3U - exponent))};
   } else {
      target = uint256{coefficient} * (uint256{1} << (8U * (exponent - 3U)));
   }
   return target;
}

uint256 difficulty_1_target() {
   // Bitcoin difficulty-1 target = 0x00000000FFFF0000.... derived from
   // 0x1d00ffff
   return expand_compact_target(0x1d00ffffU);
}

uint256 share_target_from_difficulty(double difficulty) {
   if (!(difficulty > 0.0) || !std::isfinite(difficulty)) {
      throw std::invalid_argument("difficulty must be finite and > 0");
   }

   const long double diff1 = static_cast<long double>(difficulty_1_target());
   const long double scaled = diff1 / static_cast<long double>(difficulty);

   if (!(scaled >= 0.0L)) {
      throw std::invalid_argument("invalid scaled share target");
   }

   // Truncate toward zero, which is conservative for acceptance.
   const auto as_u64 = static_cast<std::uint64_t>(scaled);
   uint256 approx{as_u64};

   // This is exact only while the result fits in 64 bits, and approximate
   // otherwise due to long double precision limits.
   //
   // For the current educational CKPool setup with small Stratum difficulty
   // values, this is fine as a first step. A fully exact path should be added
   // later using integer arithmetic.
   if (scaled >
       static_cast<long double>(std::numeric_limits<std::uint64_t>::max())) {
      // Fall back to repeated halving-style approximation through uint256
      // via long double decomposition.
      long double x = scaled;
      uint256 result{};
      for (int bit = 255; bit >= 0; --bit) {
         const long double place = std::ldexp(1.0L, bit);
         if (x >= place) {
            result += (uint256{1} << static_cast<unsigned>(bit));
            x -= place;
         }
      }
      return result;
   }

   return approx;
}

uint256 share_target_from_difficulty(std::uint64_t difficulty) {
   if (difficulty == 0U) {
      throw std::invalid_argument("difficulty must be > 0");
   }
   return difficulty_1_target() / uint256{difficulty};
}

uint256 hash_digest_to_uint256(const sha256::DigestBytes& digest_bytes) {
   // SHA-256 digest bytes are produced in conventional big-endian digest order.
   // Bitcoin target comparison treats the hash as a little-endian 256-bit
   // integer.
   return uint256::from_bytes_le(digest_bytes.data());
}

bool hash_meets_target(const sha256::DigestBytes& digest_bytes,
                       const uint256& target) {
   return hash_digest_to_uint256(digest_bytes) <= target;
}

} // namespace cpu_miner

