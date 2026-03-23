// src/util/endian.hpp

#ifndef CPU_MINER_UTIL_ENDIAN_HPP
#define CPU_MINER_UTIL_ENDIAN_HPP

#include <bit>
#include <cstdint>
#include <span>
#include <utility>

namespace cpu_miner::util {

// --- feature detection ---
#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
#define CPU_MINER_HAS_BYTESWAP 1
#else
#define CPU_MINER_HAS_BYTESWAP 0
#endif

// --- raw byte swaps (mechanical) ---

[[nodiscard]] constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {
#if CPU_MINER_HAS_BYTESWAP
   return std::byteswap(v);
#else
   return ((v & 0x000000ffU) << 24U) |
          ((v & 0x0000ff00U) << 8U) |
          ((v & 0x00ff0000U) >> 8U) |
          ((v & 0xff000000U) >> 24U);
#endif
}

[[nodiscard]] constexpr std::uint64_t bswap64(std::uint64_t v) noexcept {
#if CPU_MINER_HAS_BYTESWAP
   return std::byteswap(v);
#else
   return ((v & 0x00000000000000ffULL) << 56U) |
          ((v & 0x000000000000ff00ULL) << 40U) |
          ((v & 0x0000000000ff0000ULL) << 24U) |
          ((v & 0x00000000ff000000ULL) << 8U)  |
          ((v & 0x000000ff00000000ULL) >> 8U)  |
          ((v & 0x0000ff0000000000ULL) >> 24U) |
          ((v & 0x00ff000000000000ULL) >> 40U) |
          ((v & 0xff00000000000000ULL) >> 56U);
#endif
}

// --- semantic conversions (what you actually mean) ---

// Bitcoin header fields are serialized little-endian, but SHA-256
// operates on big-endian 32-bit words. This converts a LE header
// field into the corresponding SHA message word.
[[nodiscard]] constexpr std::uint32_t
header_le32_to_sha_word(std::uint32_t v) noexcept {
   return bswap32(v);
}

// Generic helpers if you want them (optional but useful)

[[nodiscard]] constexpr std::uint32_t le32_to_be32(std::uint32_t v) noexcept {
   return bswap32(v);
}

[[nodiscard]] constexpr std::uint32_t be32_to_le32(std::uint32_t v) noexcept {
   return bswap32(v);
}

[[nodiscard]] constexpr std::uint64_t le64_to_be64(std::uint64_t v) noexcept {
   return bswap64(v);
}

[[nodiscard]] constexpr std::uint64_t be64_to_le64(std::uint64_t v) noexcept {
   return bswap64(v);
}

inline void byteswap_each_u32(std::span<std::uint8_t, 32> bytes) noexcept {
   for (std::size_t i = 0; i < bytes.size(); i += 4U) {
      std::swap(bytes[i + 0], bytes[i + 3]);
      std::swap(bytes[i + 1], bytes[i + 2]);
   }
}

} // namespace cpu_miner::util

#endif

