#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "mining_job/header.hpp"
#include "util/hex.hpp"

TEST_CASE("sha-input header bytes match rejected fixture", "[header]") {
   using namespace cpu_miner;

   HashBytes prevhash_sha_input{};
   HashBytes merkle_root_sha_input{};

   const auto prevhash_rpc = hex_to_bytes(
      "60a003fb36548de6ed395d227308a62488b0d1c5000169d70000000000000000");
   const auto merkle_root_raw = hex_to_bytes(
      "99da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c5ff10fec");

   REQUIRE(prevhash_rpc.size() == 32U);
   REQUIRE(merkle_root_raw.size() == 32U);

   for (std::size_t i = 0; i < 32U; ++i) {
      prevhash_sha_input[i] = prevhash_rpc[i];
      merkle_root_sha_input[i] = merkle_root_raw[i];
   }

   for (std::size_t i = 0; i < 32U; i += 4U) {
      std::swap(prevhash_sha_input[i + 0U], prevhash_sha_input[i + 3U]);
      std::swap(prevhash_sha_input[i + 1U], prevhash_sha_input[i + 2U]);
   }

   const auto header =
      make_sha_input_header_bytes(u32_from_hex_be("20000000"),
                                  prevhash_sha_input, merkle_root_sha_input,
                                  u32_from_hex_be("69bc9e60"),
                                  u32_from_hex_be("1701f0cc"),
                                  u32_from_hex_be("0525050b"));

   REQUIRE(header_sha_input_hex(header) ==
           "00000020fb03a060e68d5436225d39ed24a60873c5d1b088d769010000000000"
           "0000000099da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c"
           "5ff10fec609ebc69ccf001170b052505");
}
