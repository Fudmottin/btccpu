// src/util/hex.hpp

#ifndef CPU_MINER_UTIL_HEX_HPP
#define CPU_MINER_UTIL_HEX_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace cpu_miner {

bool is_lower_hex_digit(char ch);
bool is_lower_hex_string(std::string_view s);
std::string hex_from_u64_be(std::uint64_t value, std::size_t size_bytes);

} // namespace cpu_miner

#endif

