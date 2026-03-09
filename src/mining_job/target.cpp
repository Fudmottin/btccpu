// src/mining_job/target.cpp

#include "mining_job/target.hpp"
#include "util/uint256.hpp"

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
      target =
         uint256{coefficient} * (uint256{1} << (8U * (exponent - 3U)));
   }
   return target;
}

} // namespace cpu_miner

