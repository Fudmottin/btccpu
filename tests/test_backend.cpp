#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "mining_job/cpu_backend.hpp"
#include "mining_job/target.hpp"
#include "mining_job/work_state.hpp"
#include "util/hex.hpp"

namespace {

cpu_miner::MiningJob make_accepted_job() {
   cpu_miner::MiningJob job;
   job.job_id = "69b23e1000005c34";
   job.prevhash =
      "e51ad5fa5621c25d2acddbdf84d450603f465ea30000f5080000000000000000";
   job.coinb1 =
      "0100000001000000000000000000000000000000000000000000000000000000"
      "0000000000ffffffff3503405d0e0004f7cebc6904d997a02810";
   job.coinb2 =
      "0a636b706f6f6c0d2f42697441786520427272722fffffffff02288aa1120000"
      "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
      "00266a24aa21a9ed7f45ecf1c44f416bda0266b14e8f3e6c553963a34bee620a"
      "6d34656dee710bd000000000";
   job.merkle_branch = {
      "2cf3b8ed87f898203740953a940da8c0a6dfff5e6bd108c622f26a50a1658ffc",
      "7588f0078cd6500322384afe2a06bbdc1bfb3377534f973e0286877a5072e872",
      "de732123dfcb32c200d92937bae3b8d19ddf5490e420714bb3d14c1a91478d8f",
      "f0d85f993d2739e0fa0469b59fcda525eadf7b31c9b573bf7345b1f7bd7f1479",
      "57be3e820b0fd801617120f0098807b300bb810d5629cd3a73e7d29ab4cab121",
      "60469fdb5d1e072618c31b87aa323cadb355d516124bc7abe5765a53df1627bf",
      "c631ecc346327084d6d9ec866852c9fb7ecc76632c374dac3b79a40a68bae7b1",
   };
   job.version = "20000000";
   job.nbits = "1701f0cc";
   job.ntime = "69bccef7";
   job.clean_jobs = false;
   return job;
}

cpu_miner::SubscriptionContext make_accepted_subscription() {
   cpu_miner::SubscriptionContext sub;
   sub.extranonce1 = "3f3eb26900000000";
   sub.extranonce2_size = 8U;
   return sub;
}

} // namespace

TEST_CASE("cpu backend scans prepared work and reports a share candidate",
          "[backend]") {
   using namespace cpu_miner;

   const auto prepared =
      prepare_work(make_accepted_job(), make_accepted_subscription(), 0U);
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
