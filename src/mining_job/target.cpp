// src/mining_job/target.cpp

#include <algorithm>
#include <cmath>
#include <cstdint>
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

   // Fixed-point rational approximation:
   // target = floor(diff1 / difficulty)
   //        = floor((diff1 << 32) / round(difficulty * 2^32))
   static constexpr long double kScale =
      4294967296.0L; // 2^32

   const long double scaled_ld =
      static_cast<long double>(difficulty) * kScale;

   if (!(scaled_ld >= 1.0L) ||
       scaled_ld >
          static_cast<long double>(std::numeric_limits<std::uint64_t>::max())) {
      throw std::invalid_argument("difficulty is out of supported range");
   }

   const auto scaled =
      static_cast<std::uint64_t>(std::llround(scaled_ld));

   if (scaled == 0U) {
      throw std::invalid_argument("difficulty scaling underflow");
   }

   return (difficulty_1_target() << 32U) / uint256{scaled};
}

uint256 share_target_from_difficulty(std::uint64_t difficulty) {
   if (difficulty == 0U) {
      throw std::invalid_argument("difficulty must be > 0");
   }

   return share_target_from_difficulty(static_cast<double>(difficulty));
}

uint256 hash_digest_to_uint256(const sha256::DigestBytes& digest_bytes) {
   return uint256::from_bytes_le(digest_bytes.data());
}

bool hash_meets_target(const sha256::DigestBytes& digest_bytes,
                       const uint256& target) {
   return hash_digest_to_uint256(digest_bytes) <= target;
}

} // namespace cpu_miner

