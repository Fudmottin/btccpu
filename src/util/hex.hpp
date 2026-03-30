// src/util/hex.hpp

#ifndef CPU_MINER_UTIL_HEX_HPP
#define CPU_MINER_UTIL_HEX_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_miner {

[[nodiscard]] bool is_lower_hex_digit(char ch);
[[nodiscard]] bool is_lower_hex_string(std::string_view s);

[[nodiscard]] std::string hex_from_u32_be(std::uint32_t value);
[[nodiscard]] std::string hex_from_u32_le(std::uint32_t value);
[[nodiscard]] std::string hex_from_u64_be(std::uint64_t value,
                                          std::size_t size_bytes);

[[nodiscard]] std::vector<std::uint8_t> hex_to_bytes(std::string_view hex);
[[nodiscard]] std::array<std::uint8_t, 32>
hex_to_array_32(std::string_view hex);
[[nodiscard]] std::string bytes_to_hex(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::uint32_t u32_from_hex_be(std::string_view hex);
[[nodiscard]] std::uint32_t u32_from_hex_le(std::string_view hex);

} // namespace cpu_miner

namespace cpu_miner {

template<class Range>
inline std::string bytes_to_hex_fixed_msb(const Range& bytes) {
   std::string hex;
   hex.reserve(bytes.size() * 2U);

   static constexpr char digits[] = "0123456789abcdef";

   for (auto it = bytes.rbegin(); it != bytes.rend(); ++it) {
      const std::uint8_t b = *it;
      hex.push_back(digits[(b >> 4U) & 0x0fU]);
      hex.push_back(digits[b & 0x0fU]);
   }

   return hex;
}

} // namespace cpu_miner

#endif
