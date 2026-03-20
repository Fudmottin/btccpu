// src/mining_job/header.cpp

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "mining_job/header.hpp"
#include "util/endian.hpp"
#include "util/hex.hpp"

namespace cpu_miner {
namespace {

void write_le32(std::uint32_t v, std::uint8_t* out) noexcept {
   out[0] = static_cast<std::uint8_t>(v);
   out[1] = static_cast<std::uint8_t>(v >> 8);
   out[2] = static_cast<std::uint8_t>(v >> 16);
   out[3] = static_cast<std::uint8_t>(v >> 24);
}

std::uint32_t read_be32(const std::uint8_t* in) noexcept {
   return (static_cast<std::uint32_t>(in[0]) << 24U) |
          (static_cast<std::uint32_t>(in[1]) << 16U) |
          (static_cast<std::uint32_t>(in[2]) << 8U) |
          (static_cast<std::uint32_t>(in[3]) << 0U);
}

void write_hash_bytes(std::uint8_t* out, const HashBytes& hash) noexcept {
   for (std::size_t i = 0; i < hash.size(); ++i) {
      out[i] = hash[i];
   }
}

sha256::BlockWords block_words_from_bytes(const std::uint8_t* in) noexcept {
   sha256::BlockWords block{};

   for (std::size_t i = 0; i < block.size(); ++i) {
      block[i] = read_be32(&in[i * 4U]);
   }

   return block;
}

} // namespace

HeaderBytes
make_sha_input_header_bytes(std::uint32_t version, const HashBytes& prevhash,
                            const HashBytes& merkle_root, std::uint32_t ntime,
                            std::uint32_t nbits, std::uint32_t nonce) {
   HeaderBytes header{};

   std::size_t offset = 0;

   write_le32(version, &header[offset]);
   offset += 4U;

   write_hash_bytes(&header[offset], prevhash);
   offset += 32U;

   write_hash_bytes(&header[offset], merkle_root);
   offset += 32U;

   write_le32(ntime, &header[offset]);
   offset += 4U;

   write_le32(nbits, &header[offset]);
   offset += 4U;

   write_le32(nonce, &header[offset]);

   return header;
}

std::string header_hex(const HeaderBytes& header) {
   return bytes_to_hex(header);
}

HeaderTemplate make_header_template(std::uint32_t version,
                                    const HashBytes& prevhash,
                                    const HashBytes& merkle_root,
                                    std::uint32_t ntime, std::uint32_t nbits,
                                    std::uint32_t nonce) {
   HeaderTemplate header{};

   const HeaderBytes bytes =
      make_sha_input_header_bytes(version, prevhash, merkle_root, ntime, nbits,
                                  nonce);

   header.block0 = block_words_from_bytes(bytes.data());
   header.block1 = block_words_from_bytes(bytes.data() + 64U);
   header.block1[4] = 0x80000000U;
   header.block1[5] = 0U;
   header.block1[6] = 0U;
   header.block1[7] = 0U;
   header.block1[8] = 0U;
   header.block1[9] = 0U;
   header.block1[10] = 0U;
   header.block1[11] = 0U;
   header.block1[12] = 0U;
   header.block1[13] = 0U;
   header.block1[14] = 0U;
   header.block1[15] = 0x00000280U;

   header.midstate =
      sha256::compress_block(sha256::initial_state(), header.block0);

   return header;
}

void set_header_nonce(HeaderTemplate& header, std::uint32_t nonce) noexcept {
   using cpu_miner::util::header_le32_to_sha_word;

   header.block1[3] = header_le32_to_sha_word(nonce);
}

sha256::DigestWords hash_header_template(const HeaderTemplate& header) {
   return sha256::dbl_sha256_two_block_header(header.midstate, header.block1);
}

} // namespace cpu_miner

