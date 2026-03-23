#include <catch2/catch_test_macros.hpp>
#include <string>

#include "util/hex.hpp"

TEST_CASE("hex roundtrip", "[hex]") {
   using namespace cpu_miner;

   const std::string hex = "deadbeef";
   const auto bytes = hex_to_bytes(hex);
   const auto out = bytes_to_hex(bytes);

   REQUIRE(out == hex);
}
