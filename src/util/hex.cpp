// src/util/hex.cpp

#include "util/hex.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cpu_miner {
namespace {

constexpr char hex_digits[] = "0123456789abcdef";

int hex_value(char ch) noexcept {
   if (ch >= '0' && ch <= '9') return ch - '0';
   if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
   return -1;
}

std::string hex_from_u32_impl(std::uint32_t value, bool little_endian) {
   std::string out(8U, '0');

   for (std::size_t byte = 0; byte < 4U; ++byte) {
      const std::size_t src_byte = little_endian ? byte : (3U - byte);
      const auto shift = static_cast<unsigned>(src_byte * 8U);
      const auto b = static_cast<std::uint8_t>((value >> shift) & 0xffU);

      out[byte * 2U + 0U] = hex_digits[(b >> 4U) & 0x0fU];
      out[byte * 2U + 1U] = hex_digits[b & 0x0fU];
   }

   return out;
}

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

std::string hex_from_u32_be(std::uint32_t value) {
   return hex_from_u32_impl(value, false);
}

std::string hex_from_u32_le(std::uint32_t value) {
   return hex_from_u32_impl(value, true);
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

std::vector<std::uint8_t> hex_to_bytes(std::string_view hex) {
   if (!is_lower_hex_string(hex)) {
      throw std::invalid_argument(
         "hex_to_bytes requires lowercase even-length hex");
   }

   std::vector<std::uint8_t> out;
   out.reserve(hex.size() / 2U);

   for (std::size_t i = 0; i < hex.size(); i += 2U) {
      const int hi = hex_value(hex[i]);
      const int lo = hex_value(hex[i + 1U]);

      if (hi < 0 || lo < 0) {
         throw std::invalid_argument(
            "hex_to_bytes encountered invalid hex digit");
      }

      out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
   }

   return out;
}

std::array<std::uint8_t, 32> hex_to_array_32(std::string_view hex) {
   const auto bytes = hex_to_bytes(hex);
   if (bytes.size() != 32U) {
      throw std::invalid_argument("expected 32-byte hash hex");
   }

   std::array<std::uint8_t, 32> out{};
   for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = bytes[i];
   }
   return out;
}

std::string bytes_to_hex(std::span<const std::uint8_t> bytes) {
   std::string out;
   out.resize(bytes.size() * 2U);

   for (std::size_t i = 0; i < bytes.size(); ++i) {
      const std::uint8_t byte = bytes[i];
      out[i * 2U] = hex_digits[(byte >> 4U) & 0x0fU];
      out[i * 2U + 1U] = hex_digits[byte & 0x0fU];
   }

   return out;
}

std::uint32_t u32_from_hex_be(std::string_view hex) {
   if (hex.size() != 8U) {
      throw std::invalid_argument("u32_from_hex_be requires 8 hex chars");
   }
   if (!is_lower_hex_string(hex)) {
      throw std::invalid_argument("u32_from_hex_be requires lowercase hex");
   }

   std::uint32_t value = 0U;
   for (char ch : hex) {
      const int v = hex_value(ch);
      if (v < 0) {
         throw std::invalid_argument(
            "u32_from_hex_be encountered invalid hex digit");
      }
      value = static_cast<std::uint32_t>((value << 4U) |
                                         static_cast<std::uint32_t>(v));
   }
   return value;
}

std::uint32_t u32_from_hex_le(std::string_view hex) {
   if (hex.size() != 8U) {
      throw std::invalid_argument("u32_from_hex_le requires 8 hex chars");
   }
   if (!is_lower_hex_string(hex)) {
      throw std::invalid_argument("u32_from_hex_le requires lowercase hex");
   }

   const auto bytes = hex_to_bytes(hex);

   return static_cast<std::uint32_t>(bytes[0]) |
          (static_cast<std::uint32_t>(bytes[1]) << 8U) |
          (static_cast<std::uint32_t>(bytes[2]) << 16U) |
          (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

} // namespace cpu_miner
