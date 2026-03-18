// src/util/hex.hpp

#ifndef CPU_MINER_UTIL_HEX_HPP
#define CPU_MINER_UTIL_HEX_HPP

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
[[nodiscard]] std::string bytes_to_hex(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::uint32_t u32_from_hex_be(std::string_view hex);
[[nodiscard]] std::uint32_t u32_from_hex_le(std::string_view hex);

} // namespace cpu_miner

#endif

