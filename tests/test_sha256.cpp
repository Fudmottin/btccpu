#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "sha256/sha256.hpp"

TEST_CASE("sha256 basic shape", "[sha256]") {
   using namespace cpu_miner::sha256;

   const std::array<std::uint8_t, 3> data{'a', 'b', 'c'};
   const auto digest = sha256_words(data);

   REQUIRE(digest.size() == 8U);
}
