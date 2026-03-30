#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "mining_job/cpu_backend.hpp"
#include "mining_job/target.hpp"
#include "mining_job/work_state.hpp"
#include "support/accepted_fixture.hpp"
#include "util/hex.hpp"

TEST_CASE("cpu backend scans prepared work and reports a share candidate",
          "[backend]") {
   using namespace cpu_miner;

   const auto prepared =
      prepare_work(test_support::make_accepted_job(),
                   test_support::make_accepted_subscription(), 0U);
   const auto target_nonce = u32_from_hex_be("00293f3b");

   BackendScanRequest request{
      .prepared = prepared,
      .network_target =
         expand_compact_target(u32_from_hex_be(prepared.job.nbits)),
      .share_target = share_target_from_difficulty(std::uint64_t{1}),
      .nonce_begin = target_nonce,
      .nonce_end = target_nonce,
      .progress_interval = 0U,
      .control = {},
   };

   CpuHasherBackend backend;
   std::vector<ShareCandidate> shares;

   const auto result =
      backend.scan(request, [&](const ShareCandidate& candidate) {
         shares.push_back(candidate);
      });

   REQUIRE(backend.name() == "cpu");
   REQUIRE(result.hashes_done == 1U);
   REQUIRE(result.shares_found == 1U);
   REQUIRE(result.blocks_found == 0U);
   REQUIRE(shares.size() == 1U);

   REQUIRE(shares[0].nonce == target_nonce);
   REQUIRE(bytes_to_hex(shares[0].hash) ==
           "8bb6fe2d423e1030ca773a9e0f459f22bbbdb2f63bae0645e6118ca700000000");
   REQUIRE_FALSE(shares[0].is_block_candidate);
   REQUIRE(shares[0].work.coinbase.extranonce2_hex == "0000000000000000");
}
