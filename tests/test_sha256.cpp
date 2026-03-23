#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "sha256/sha256.hpp"
#include "util/hex.hpp"

TEST_CASE("sha256 abc test vector", "[sha256]") {
   using namespace cpu_miner;

   const std::array<std::uint8_t, 3> data{'a', 'b', 'c'};
   const auto digest = sha256::sha256_words(data);
   const auto digest_hex =
      bytes_to_hex(sha256::digest_words_to_bytes_be(digest));

   REQUIRE(digest_hex ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
