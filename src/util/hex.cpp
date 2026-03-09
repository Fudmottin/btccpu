// src/util/hex.cpp

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "util/hex.hpp"

namespace cpu_miner {
namespace {

constexpr char hex_digits[] = "0123456789abcdef";

} // namespace

bool is_lower_hex_digit(char ch) {
   return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

bool is_lower_hex_string(std::string_view s) {
   if ((s.size() % 2U) != 0U) return false;

   for (char ch : s) {
      if (!is_lower_hex_digit(ch)) return false;
   }
   return true;
}

std::string hex_from_u64_be(std::uint64_t value, std::size_t size_bytes) {
   if (size_bytes > sizeof(value)) {
      throw std::invalid_argument("size_bytes exceeds uint64_t width");
   }

   std::string out(size_bytes * 2U, '0');

   for (std::size_t i = 0; i < size_bytes; ++i) {
      const std::size_t shift_bytes = size_bytes - 1U - i;
      const auto shift_bits =
         static_cast<unsigned>(shift_bytes * static_cast<std::size_t>(8U));
      const std::uint8_t byte = static_cast<std::uint8_t>(value >> shift_bits);

      out[i * 2U] = hex_digits[(byte >> 4U) & 0x0fU];
      out[i * 2U + 1U] = hex_digits[byte & 0x0fU];
   }

   return out;
}

} // namespace cpu_miner

