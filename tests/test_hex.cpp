#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>

#include "util/hex.hpp"

TEST_CASE("hex roundtrip", "[hex]") {
   using namespace cpu_miner;

   const std::string hex = "deadbeef";
   const auto bytes = hex_to_bytes(hex);
   const auto out = bytes_to_hex(bytes);

   REQUIRE(out == hex);
}

TEST_CASE("u32 hex conversions preserve endianness intent", "[hex]") {
   using namespace cpu_miner;

   constexpr std::uint32_t value = 0x12345678U;

   REQUIRE(hex_from_u32_be(value) == "12345678");
   REQUIRE(hex_from_u32_le(value) == "78563412");

   REQUIRE(u32_from_hex_be("12345678") == value);
   REQUIRE(u32_from_hex_le("78563412") == value);
}
