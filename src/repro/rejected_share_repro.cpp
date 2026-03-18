// src/repro/rejected_share_repro.cpp

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "mining_job/header.hpp"
#include "mining_job/target.hpp"
#include "sha256/sha256.hpp"
#include "util/hex.hpp"

namespace {

using cpu_miner::HashBytes;
using cpu_miner::uint256;

enum class Transform32 {
   identity,
   reverse_32,
   swap_u32_bytes,
};

HashBytes hash32_from_hex(std::string_view hex) {
   const auto bytes = cpu_miner::hex_to_bytes(hex);
   if (bytes.size() != 32U) {
      throw std::invalid_argument("expected 32-byte hash hex");
   }

   HashBytes out{};
   for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = bytes[i];
   }

   return out;
}

HashBytes apply_transform(HashBytes in, Transform32 t) {
   switch (t) {
   case Transform32::identity:
      return in;

   case Transform32::reverse_32:
      std::reverse(in.begin(), in.end());
      return in;

   case Transform32::swap_u32_bytes:
      for (std::size_t i = 0; i < in.size(); i += 4U) {
         std::swap(in[i + 0U], in[i + 3U]);
         std::swap(in[i + 1U], in[i + 2U]);
      }
      return in;
   }

   return in;
}

const char* transform_name(Transform32 t) {
   switch (t) {
   case Transform32::identity:
      return "identity";
   case Transform32::reverse_32:
      return "reverse_32";
   case Transform32::swap_u32_bytes:
      return "swap_u32_bytes";
   }

   return "unknown";
}

std::string bytes_to_hex_reversed(std::span<const std::uint8_t> bytes) {
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

std::string bytes_to_hex_forward(std::span<const std::uint8_t> bytes) {
   return cpu_miner::bytes_to_hex(bytes);
}

std::string digest_hex_msb(const cpu_miner::sha256::DigestBytes& digest) {
   return bytes_to_hex_reversed(digest);
}

HashBytes dbl_sha256_bytes(std::span<const std::uint8_t> bytes) {
   const auto words = cpu_miner::sha256::dbl_sha256_words(bytes);
   return cpu_miner::sha256::digest_words_to_bytes_be(words);
}

uint256 hash_as_le_uint256(const HashBytes& hash_bytes) {
   return uint256::from_bytes_le(hash_bytes.data());
}

uint256 hash_as_be_uint256(HashBytes hash_bytes) {
   std::reverse(hash_bytes.begin(), hash_bytes.end());
   return uint256::from_bytes_le(hash_bytes.data());
}

HashBytes merkle_fold_variant(HashBytes current_hash,
                              const std::vector<std::string>& merkle_branch,
                              Transform32 branch_transform) {
   std::array<std::uint8_t, 64> buffer{};

   for (const auto& branch_hex : merkle_branch) {
      const auto branch =
         apply_transform(hash32_from_hex(branch_hex), branch_transform);

      for (std::size_t i = 0; i < 32U; ++i) {
         buffer[i] = current_hash[i];
         buffer[32U + i] = branch[i];
      }

      current_hash = dbl_sha256_bytes(buffer);
   }

   return current_hash;
}

void run_variant(std::string_view label, std::string_view prevhash_hex,
                 std::string_view coinbase_hex,
                 const std::vector<std::string>& merkle_branch,
                 Transform32 coinbase_transform, Transform32 prevhash_transform,
                 Transform32 branch_transform,
                 Transform32 final_merkle_transform, std::uint32_t version,
                 std::uint32_t ntime, std::uint32_t nbits, std::uint32_t nonce,
                 const uint256& share_target, const uint256& network_target) {
   const auto coinbase_bytes = cpu_miner::hex_to_bytes(coinbase_hex);
   const auto coinbase_hash_raw = dbl_sha256_bytes(coinbase_bytes);
   const auto coinbase_hash =
      apply_transform(coinbase_hash_raw, coinbase_transform);

   const auto prevhash =
      apply_transform(hash32_from_hex(prevhash_hex), prevhash_transform);

   const auto merkle_raw =
      merkle_fold_variant(coinbase_hash, merkle_branch, branch_transform);
   const auto merkle_root = apply_transform(merkle_raw, final_merkle_transform);

   const auto header =
      cpu_miner::make_header_bytes(version, prevhash, merkle_root, ntime, nbits,
                                   nonce);

   const auto hash_words = cpu_miner::sha256::dbl_sha256_words(header);
   const auto hash_bytes =
      cpu_miner::sha256::digest_words_to_bytes_be(hash_words);

   const auto hash_le = hash_as_le_uint256(hash_bytes);
   const auto hash_be = hash_as_be_uint256(hash_bytes);

   const bool meets_share_le = hash_le <= share_target;
   const bool meets_share_be = hash_be <= share_target;
   const bool meets_network_le = hash_le <= network_target;
   const bool meets_network_be = hash_be <= network_target;

   std::cout << "  coinbase hash display: "
             << bytes_to_hex_reversed(coinbase_hash_raw) << '\n';
   std::cout << "variant: " << label << '\n';
   std::cout << "  coinbase transform:   " << transform_name(coinbase_transform)
             << '\n';
   std::cout << "  prevhash transform:   " << transform_name(prevhash_transform)
             << '\n';
   std::cout << "  branch transform:     " << transform_name(branch_transform)
             << '\n';
   std::cout << "  final merkle xform:   "
             << transform_name(final_merkle_transform) << '\n';
   std::cout << "  merkle root display:  " << bytes_to_hex_reversed(merkle_root)
             << '\n';
   std::cout << "  header hex:           " << cpu_miner::header_hex(header)
             << '\n';
   std::cout << "  hash bytes forward:   " << bytes_to_hex_forward(hash_bytes)
             << '\n';
   std::cout << "  header hash display:  " << digest_hex_msb(hash_bytes)
             << '\n';
   std::cout << "  hash uint256 le:      " << hash_le.to_hex_be_fixed() << '\n';
   std::cout << "  hash uint256 be:      " << hash_be.to_hex_be_fixed() << '\n';
   std::cout << "  meets share target:   "
             << (cpu_miner::hash_meets_target(hash_bytes, share_target)
                    ? "true"
                    : "false")
             << '\n';
   std::cout << "  meets share target le:"
             << (meets_share_le ? " true" : " false") << '\n';
   std::cout << "  meets share target be:"
             << (meets_share_be ? " true" : " false") << '\n';
   std::cout << "  meets net target le:  "
             << (meets_network_le ? "true" : "false") << '\n';
   std::cout << "  meets net target be:  "
             << (meets_network_be ? "true" : "false") << '\n';
   std::cout << '\n';
}

} // namespace

int main() {
   try {
      /*************************************************************************
      Captured rejected share:
        generation: 9
        job_id:     69b23e100000458d
        ntime:      69ba3257
        nonce:      99867158
        nonce_hex:  05f3da16
        result:     rejected as "Above target"
      *************************************************************************/

      const std::string job_id = "69b23e100000458d";
      const std::string prevhash_hex =
         "0b60966e8ba272edc094e436972f5393cdc5b07d00000c330000000000000000";
      const std::string coinb1 =
         "0100000001000000000000000000000000000000000000000000000000000000"
         "0000000000ffffffff3503325c0e00045732ba690437bd601710";
      const std::string extranonce1 = "3b3eb26900000000";
      const std::string extranonce2 = "0000000000000000";
      const std::string coinb2 =
         "0a636b706f6f6c0d2f42697441786520427272722fffffffff026d97ae120000"
         "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
         "00266a24aa21a9edef56a4295718f9bcca6bdd8995d7363f2276443e1c91bcf7"
         "8ce9d15b6f8d8bff00000000";

      const std::vector<std::string> merkle_branch = {
         "64a87c6bc9ad703a6510b545d6f7ed3cf6edef1495303bb2bb0452c129d0f0e9",
         "56a7e5b2c01038c1644a765543521ce531c3251ecb13f1655e262254c77b1d20",
         "ef43eeedcc19edc3917b788d80c75f0a3f49774e061dac75dcb51cc887796d7b",
         "d6b86633337d8abcb148e5276e3071f8259ed2ae887e12d4914a5a226a088571",
         "4fbea83eb045907b5009119775ed62e44456cff0e345634f4314721a988b9a24",
         "df16cbc05d6efc3b4b04429e63e44075fcffd9f0d844342cf9935ba6183b21d3",
         "a3a1da21094fec0e3891309a644849c99cc69050ca558c8d995decb15ceb8de6",
         "a4c622bc0527fe759e46fb857fbbbc0a6baf2b1fd560bd2b0f137f89e12ed8ab",
         "09f9d31ac963272608cf550c1a820f7aa17bc43b6d7a9ff69a610b3906663423",
         "abe9b9993f2d4690fc57dcf199d00c3be1a66eca5291a954c9ebf3bd581bd76b",
         "207ebd2aa146c06a20fb13947f4391da9f45a2435ae57bc97d300064449bd91d",
      };

      const std::string coinbase_hex =
         coinb1 + extranonce1 + extranonce2 + coinb2;

      const std::uint32_t version = cpu_miner::u32_from_hex_be("20000000");
      const std::uint32_t nbits = cpu_miner::u32_from_hex_be("1701f0cc");
      const std::uint32_t ntime = cpu_miner::u32_from_hex_be("69ba3257");
      const std::uint32_t nonce = cpu_miner::u32_from_hex_be("05f3da16");

      const auto share_target = cpu_miner::share_target_from_difficulty(1ULL);
      const auto network_target = cpu_miner::expand_compact_target(nbits);

      std::cout << "job_id: " << job_id << '\n';
      std::cout << "coinbase hex chars: " << coinbase_hex.size() << '\n';
      std::cout << "share target:   " << share_target.to_hex_be_fixed() << '\n';
      std::cout << "network target: " << network_target.to_hex_be_fixed()
                << "\n\n";

      run_variant("current-code-like", prevhash_hex, coinbase_hex,
                  merkle_branch, Transform32::identity,
                  Transform32::swap_u32_bytes, Transform32::identity,
                  Transform32::swap_u32_bytes, version, ntime, nbits, nonce,
                  share_target, network_target);

      run_variant("grok-like", prevhash_hex, coinbase_hex, merkle_branch,
                  Transform32::identity, Transform32::reverse_32,
                  Transform32::reverse_32, Transform32::identity, version,
                  ntime, nbits, nonce, share_target, network_target);

      run_variant("current + reverse-coinbase-leaf", prevhash_hex, coinbase_hex,
                  merkle_branch, Transform32::reverse_32,
                  Transform32::swap_u32_bytes, Transform32::identity,
                  Transform32::swap_u32_bytes, version, ntime, nbits, nonce,
                  share_target, network_target);

      run_variant("current + swap-coinbase-leaf", prevhash_hex, coinbase_hex,
                  merkle_branch, Transform32::swap_u32_bytes,
                  Transform32::swap_u32_bytes, Transform32::identity,
                  Transform32::swap_u32_bytes, version, ntime, nbits, nonce,
                  share_target, network_target);

      return 0;
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

