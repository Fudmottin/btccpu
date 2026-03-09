// src/sha256/sha256.cpp

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "sha256/sha256.hpp"

namespace cpu_miner::sha256 {
namespace {

using Message = std::vector<std::uint8_t>;

// Section 4.4.2 SHA-256 constants.
constexpr std::array<Word, 64> K = {
   0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
   0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
   0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
   0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
   0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
   0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
   0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
   0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
   0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
   0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
   0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

// Section 5.3.3 SHA-256 initial hash value.
constexpr DigestWords H0 = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

constexpr Word ch(Word x, Word y, Word z) noexcept {
   return (x & y) ^ ((~x) & z);
}

constexpr Word maj(Word x, Word y, Word z) noexcept {
   return (x & y) ^ (x & z) ^ (y & z);
}

constexpr Word big_sigma0(Word x) noexcept {
   return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

constexpr Word big_sigma1(Word x) noexcept {
   return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}

constexpr Word small_sigma0(Word x) noexcept {
   return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3);
}

constexpr Word small_sigma1(Word x) noexcept {
   return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10);
}

Message padding_for_bit_length(std::uint64_t bit_length) {
   const std::size_t message_bytes = static_cast<std::size_t>(bit_length / 8U);
   const std::size_t zero_pad_bytes =
      (56U + 64U - ((message_bytes + 1U) % 64U)) % 64U;

   Message padding;
   padding.reserve(1U + zero_pad_bytes + 8U);

   padding.push_back(0x80U);
   padding.insert(padding.end(), zero_pad_bytes, 0x00U);

   for (int i = 7; i >= 0; --i) {
      const auto shift = static_cast<unsigned>(i * 8);
      padding.push_back(
         static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
   }

   return padding;
}

constexpr Word read_be32(const std::uint8_t* p) noexcept {
   return (static_cast<Word>(p[0]) << 24U) | (static_cast<Word>(p[1]) << 16U) |
          (static_cast<Word>(p[2]) << 8U) | (static_cast<Word>(p[3]) << 0U);
}

constexpr void write_be32(Word value, std::uint8_t* out) noexcept {
   out[0] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
   out[1] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
   out[2] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
   out[3] = static_cast<std::uint8_t>((value >> 0U) & 0xffU);
}

constexpr void write_le32(Word value, std::uint8_t* out) noexcept {
   out[0] = static_cast<std::uint8_t>((value >> 0U) & 0xffU);
   out[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
   out[2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
   out[3] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

Schedule make_schedule(const BlockWords& block) {
   Schedule schedule{};

   std::ranges::copy(block, schedule.begin());

   for (std::size_t t = 16; t < schedule.size(); ++t) {
      schedule[t] = small_sigma1(schedule[t - 2U]) + schedule[t - 7U] +
                    small_sigma0(schedule[t - 15U]) + schedule[t - 16U];
   }

   return schedule;
}

DigestWords run_schedule(const Schedule& schedule, DigestWords digest) {
   Word a = digest[0];
   Word b = digest[1];
   Word c = digest[2];
   Word d = digest[3];
   Word e = digest[4];
   Word f = digest[5];
   Word g = digest[6];
   Word h = digest[7];

   for (std::size_t t = 0; t < schedule.size(); ++t) {
      const Word t1 = h + big_sigma1(e) + ch(e, f, g) + K[t] + schedule[t];
      const Word t2 = big_sigma0(a) + maj(a, b, c);

      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
   }

   digest[0] += a;
   digest[1] += b;
   digest[2] += c;
   digest[3] += d;
   digest[4] += e;
   digest[5] += f;
   digest[6] += g;
   digest[7] += h;

   return digest;
}

DigestWords sha256_impl(std::span<const std::uint8_t> data) {
   Message message(data.begin(), data.end());

   const std::uint64_t bit_length =
      static_cast<std::uint64_t>(message.size()) * 8U;
   const Message padding = padding_for_bit_length(bit_length);
   message.insert(message.end(), padding.begin(), padding.end());

   DigestWords digest = H0;

   for (std::size_t offset = 0; offset < message.size(); offset += 64U) {
      BlockWords block{};

      for (std::size_t i = 0; i < block.size(); ++i) {
         block[i] = read_be32(&message[offset + (i * 4U)]);
      }

      digest = run_schedule(make_schedule(block), digest);
   }

   return digest;
}

BlockWords digest_words_to_padded_block(const DigestWords& digest) noexcept {
   BlockWords block{};

   for (std::size_t i = 0; i < digest.size(); ++i) {
      block[i] = digest[i];
   }

   block[8] = 0x80000000U;
   block[9] = 0U;
   block[10] = 0U;
   block[11] = 0U;
   block[12] = 0U;
   block[13] = 0U;
   block[14] = 0U;
   block[15] = 0x00000100U;

   return block;
}

} // namespace

DigestWords initial_state() noexcept { return H0; }

DigestWords compress_block(const DigestWords& state, const BlockWords& block) {
   return run_schedule(make_schedule(block), state);
}

DigestWords hash_digest_words(const DigestWords& digest) {
   return compress_block(H0, digest_words_to_padded_block(digest));
}

DigestWords dbl_sha256_two_block_header(const DigestWords& midstate,
                                        const BlockWords& block1) {
   const DigestWords first = compress_block(midstate, block1);
   return hash_digest_words(first);
}

DigestWords sha256_words(std::span<const std::uint8_t> data) {
   return sha256_impl(data);
}

DigestWords dbl_sha256_words(std::span<const std::uint8_t> data) {
   const DigestWords first = sha256_impl(data);
   return hash_digest_words(first);
}

DigestBytes digest_words_to_bytes_be(const DigestWords& digest) noexcept {
   DigestBytes out{};

   for (std::size_t i = 0; i < digest.size(); ++i) {
      write_be32(digest[i], &out[i * 4U]);
   }

   return out;
}

DigestBytes digest_words_to_bytes_le(const DigestWords& digest) noexcept {
   DigestBytes out{};

   for (std::size_t i = 0; i < digest.size(); ++i) {
      write_le32(digest[i], &out[i * 4U]);
   }

   return out;
}

} // namespace cpu_miner::sha256

