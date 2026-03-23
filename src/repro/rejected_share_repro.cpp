// src/repro/rejected_share_repro.cpp

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
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

constexpr std::array all_transforms{
   Transform32::identity,
   Transform32::reverse_32,
   Transform32::swap_u32_bytes,
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

std::string bytes_to_hex_forward(std::span<const std::uint8_t> bytes) {
   return cpu_miner::bytes_to_hex(bytes);
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

HashBytes dbl_sha256_bytes(std::span<const std::uint8_t> bytes) {
   const auto words = cpu_miner::sha256::dbl_sha256_words(bytes);
   return cpu_miner::sha256::digest_words_to_bytes_be(words);
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

struct VariantResult {
   Transform32 coinbase_transform{};
   Transform32 prevhash_transform{};
   Transform32 branch_transform{};
   Transform32 final_merkle_transform{};

   HashBytes coinbase_hash_raw{};
   HashBytes coinbase_hash_for_tree{};
   HashBytes prevhash_for_header{};
   HashBytes merkle_root_raw{};
   HashBytes merkle_root_for_header{};
   std::array<std::uint8_t, 80> header{};
   HashBytes header_hash_raw{};

   bool meets_share{};
   bool meets_network{};
};

VariantResult
evaluate_variant(std::string_view prevhash_hex, std::string_view coinbase_hex,
                 const std::vector<std::string>& merkle_branch,
                 Transform32 coinbase_transform, Transform32 prevhash_transform,
                 Transform32 branch_transform,
                 Transform32 final_merkle_transform, std::uint32_t version,
                 std::uint32_t ntime, std::uint32_t nbits, std::uint32_t nonce,
                 const uint256& share_target, const uint256& network_target) {
   VariantResult result{};
   result.coinbase_transform = coinbase_transform;
   result.prevhash_transform = prevhash_transform;
   result.branch_transform = branch_transform;
   result.final_merkle_transform = final_merkle_transform;

   const auto coinbase_bytes = cpu_miner::hex_to_bytes(coinbase_hex);
   result.coinbase_hash_raw = dbl_sha256_bytes(coinbase_bytes);
   result.coinbase_hash_for_tree =
      apply_transform(result.coinbase_hash_raw, coinbase_transform);

   result.prevhash_for_header =
      apply_transform(hash32_from_hex(prevhash_hex), prevhash_transform);

   result.merkle_root_raw =
      merkle_fold_variant(result.coinbase_hash_for_tree, merkle_branch,
                          branch_transform);
   result.merkle_root_for_header =
      apply_transform(result.merkle_root_raw, final_merkle_transform);

   result.header =
      cpu_miner::make_sha_input_header_bytes(version,
                                             result.prevhash_for_header,
                                             result.merkle_root_for_header,
                                             ntime, nbits, nonce);

   result.header_hash_raw = dbl_sha256_bytes(result.header);
   result.meets_share =
      cpu_miner::hash_meets_target(result.header_hash_raw, share_target);
   result.meets_network =
      cpu_miner::hash_meets_target(result.header_hash_raw, network_target);

   return result;
}

void print_variant(const VariantResult& r) {
   std::cout << "alternate models producing false local share hits:\n";
   std::cout << "  coinbase transform:    "
             << transform_name(r.coinbase_transform) << '\n';
   std::cout << "  prevhash transform:    "
             << transform_name(r.prevhash_transform) << '\n';
   std::cout << "  branch transform:      "
             << transform_name(r.branch_transform) << '\n';
   std::cout << "  final merkle xform:    "
             << transform_name(r.final_merkle_transform) << '\n';

   std::cout << "  coinbase hash raw:     "
             << bytes_to_hex_forward(r.coinbase_hash_raw) << '\n';
   std::cout << "  coinbase hash display: "
             << bytes_to_hex_reversed(r.coinbase_hash_raw) << '\n';

   std::cout << "  coinbase hash in tree: "
             << bytes_to_hex_forward(r.coinbase_hash_for_tree) << '\n';

   std::cout << "  prevhash in header:    "
             << bytes_to_hex_forward(r.prevhash_for_header) << '\n';

   std::cout << "  merkle root raw:       "
             << bytes_to_hex_forward(r.merkle_root_raw) << '\n';
   std::cout << "  merkle root display:   "
             << bytes_to_hex_reversed(r.merkle_root_raw) << '\n';

   std::cout << "  merkle root in header: "
             << bytes_to_hex_forward(r.merkle_root_for_header) << '\n';

   std::cout << "  header hex:            "
             << cpu_miner::header_sha_input_hex(r.header) << '\n';

   std::cout << "  hash raw bytes:        "
             << bytes_to_hex_forward(r.header_hash_raw) << '\n';
   std::cout << "  hash display:          "
             << bytes_to_hex_reversed(r.header_hash_raw) << '\n';

   std::cout << "  meets share target:    "
             << (r.meets_share ? "true" : "false") << '\n';
   std::cout << "  meets network target:  "
             << (r.meets_network ? "true" : "false") << '\n';
   std::cout << '\n';
}

} // namespace

int main() {
   try {
      /*************************************************************************
      Live rejected share:

      generation:   86
      job_id:       69b23e1000005a88
      ntime:        69bc9e60
      nonce_hex:    0525050b
      extranonce1:  3e3eb26900000000
      extranonce2:  0000000000000000
      pool result:  rejected as "Above target"
      *************************************************************************/

      const std::string job_id = "69b23e1000005a88";

      const std::string prevhash_hex =
         "60a003fb36548de6ed395d227308a62488b0d1c5000169d70000000000000000";

      const std::string coinb1 =
         "0100000001000000000000000000000000000000000000000000000000000000"
         "0000000000ffffffff3503335d0e0004609ebc69047e483d0d10";

      const std::string extranonce1 = "3e3eb26900000000";
      const std::string extranonce2 = "0000000000000000";

      const std::string coinb2 =
         "0a636b706f6f6c0d2f42697441786520427272722fffffffff02f0c8a5120000"
         "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
         "00266a24aa21a9edae14689538cbacfe37f6e55e5acb11f6528600ede90d01f52"
         "328a7154468280700000000";

      const std::vector<std::string> merkle_branch = {
         "763fc3374b1d35c9110f4424d6a86bfc5057eab9e20419f40667b25105d37f9b",
         "19f4561c51dab0f5af712b1d833405cbb75bad91114abc575b92aa0d6eb324b0",
         "12ac175d0ae5a54a79ff439a0ad8bb1e3d9495c7226587da73c2af009db13b39",
         "836776ec508139f7dc8058c5bcc320eddaedc85bac297f3c469a080cfc4d4ed4",
         "8a58e8348bfbce05b7cd5834877be291511b9293008ad49e8ccd125cf29a6613",
         "92a866abe78e22a31c3eefa7c36e2049e6bac9fe8242eaa065414b67688a7b2b",
         "df3aa4a153486c615b79263388f1fe1d8adc48bdc0467310795da2d7d57866ad",
         "04d8a5f87d67d225ce47dd8049dba0e21eb3d6dcd34e89a90d688d721812fdff",
         "ea1b30222b36b1b7657a7777a7da0c347d2307235255379f27c2ee856dc96501",
      };

      const std::string coinbase_hex =
         coinb1 + extranonce1 + extranonce2 + coinb2;

      const std::uint32_t version = cpu_miner::u32_from_hex_be("20000000");
      const std::uint32_t nbits = cpu_miner::u32_from_hex_be("1701f0cc");
      const std::uint32_t ntime = cpu_miner::u32_from_hex_be("69bc9e60");
      const std::uint32_t nonce = cpu_miner::u32_from_hex_be("0525050b");

      const auto share_target =
         cpu_miner::share_target_from_difficulty(std::uint64_t{1});
      const auto network_target = cpu_miner::expand_compact_target(nbits);

      std::cout << "job_id: " << job_id << '\n';
      std::cout << "coinbase hex chars: " << coinbase_hex.size() << '\n';
      std::cout << "merkle branches: " << merkle_branch.size() << '\n';
      std::cout << "share target:   " << share_target.to_hex_be_fixed() << '\n';
      std::cout << "network target: " << network_target.to_hex_be_fixed()
                << "\n\n";

      std::size_t total_variants = 0;
      std::size_t share_hits = 0;
      std::size_t network_hits = 0;

      const auto current_miner = evaluate_variant(
         prevhash_hex, coinbase_hex, merkle_branch,
         Transform32::identity,       // coinbase leaf
         Transform32::swap_u32_bytes, // prevhash in SHA-input header
         Transform32::identity,       // merkle branches
         Transform32::identity,       // final merkle root in SHA-input header
         version, ntime, nbits, nonce, share_target, network_target);

      std::cout << "current miner model:\n";
      print_variant(current_miner);

      for (const auto coinbase_t : all_transforms) {
         for (const auto prevhash_t : all_transforms) {
            for (const auto branch_t : all_transforms) {
               for (const auto final_merkle_t : all_transforms) {
                  ++total_variants;

                  const auto result =
                     evaluate_variant(prevhash_hex, coinbase_hex, merkle_branch,
                                      coinbase_t, prevhash_t, branch_t,
                                      final_merkle_t, version, ntime, nbits,
                                      nonce, share_target, network_target);

                  if (!result.meets_share) {
                     continue;
                  }

                  ++share_hits;
                  if (result.meets_network) {
                     ++network_hits;
                  }

                  print_variant(result);
               }
            }
         }
      }

      std::cout << "summary:\n";
      std::cout << "  total variants:      " << total_variants << '\n';
      std::cout << "  share-target hits:   " << share_hits << '\n';
      std::cout << "  network-target hits: " << network_hits << '\n';

      return 0;
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

