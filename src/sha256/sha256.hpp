// src/sha256/sha256.hpp
#ifndef CPU_MINER_SHA256_SHA256_HPP
#define CPU_MINER_SHA256_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cpu_miner::sha256 {

using Word = std::uint32_t;
using DigestWords = std::array<Word, 8>;
using DigestBytes = std::array<std::uint8_t, 32>;
using BlockWords = std::array<Word, 16>;
using Schedule = std::array<Word, 64>;

DigestWords sha256_words(std::span<const std::uint8_t> data);
DigestWords dbl_sha256_words(std::span<const std::uint8_t> data);

DigestBytes digest_words_to_bytes_be(const DigestWords& digest) noexcept;
DigestBytes digest_words_to_bytes_le(const DigestWords& digest) noexcept;

} // namespace cpu_miner::sha256

#endif

