// src/mining_job/merkle.cpp

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "mining_job/merkle.hpp"
#include "sha256/sha256.hpp"
#include "util/hex.hpp"

namespace cpu_miner {
namespace {

HashBytes to_hash_bytes(const std::vector<std::uint8_t>& bytes) {
   if (bytes.size() != 32U) {
      throw std::invalid_argument("expected 32-byte hash");
   }

   HashBytes out{};
   for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = bytes[i];
   }
   return out;
}

} // namespace

HashBytes merkle_fold(HashBytes current_hash,
                      const std::vector<std::string>& merkle_branch) {
   std::array<std::uint8_t, 64> buffer{};

   for (const auto& branch_hex : merkle_branch) {
      const auto branch_vec = hex_to_bytes(branch_hex);
      const HashBytes branch = to_hash_bytes(branch_vec);

      for (std::size_t i = 0; i < 32U; ++i) {
         buffer[i] = current_hash[i];
         buffer[32U + i] = branch[i];
      }

      const auto folded_words = sha256::dbl_sha256_words(buffer);
      current_hash = sha256::digest_words_to_bytes_be(folded_words);
   }

   return current_hash;
}

std::string merkle_root_hex(HashBytes coinbase_hash,
                            const std::vector<std::string>& merkle_branch) {
   const HashBytes root = merkle_fold(coinbase_hash, merkle_branch);
   return bytes_to_hex(root);
}

} // namespace cpu_miner

