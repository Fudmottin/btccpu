// tests/test_coordinator.cpp

#include <catch2/catch_test_macros.hpp>

#include "mining_job/coordinator.hpp"
#include "mining_job/cpu_backend.hpp"
#include "mining_job/target.hpp"
#include "support/accepted_fixture.hpp"
#include "util/hex.hpp"

TEST_CASE("coordinator produces correct share submission", "[coordinator]") {
   using namespace cpu_miner;

   CpuHasherBackend backend;
   MiningCoordinator coordinator{backend};

   coordinator.set_job(test_support::make_accepted_job(),
                       test_support::make_accepted_subscription());

   ShareSubmission submission{};
   bool called = false;

   coordinator.on_share_found(
      [&](const ShareSubmission& s, const ShareCandidate&) {
         submission = s;
         called = true;
      });

   const auto target_nonce = u32_from_hex_be("00293f3b");

   const auto result =
      coordinator.scan_range(target_nonce, target_nonce,
                             expand_compact_target(u32_from_hex_be("1701f0cc")),
                             share_target_from_difficulty(std::uint64_t{1}), 0U,
                             ScanControl{});

   REQUIRE(result.hashes_done == 1U);
   REQUIRE(result.shares_found == 1U);
   REQUIRE(called);

   REQUIRE(submission.job_id == "69b23e1000005c34");
   REQUIRE(submission.extranonce2_hex == "0000000000000000");
   REQUIRE(submission.ntime_hex == "69bccef7");
   REQUIRE(submission.nonce_hex == "00293f3b");
}

