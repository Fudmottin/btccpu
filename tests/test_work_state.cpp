#include <catch2/catch_test_macros.hpp>

#include "mining_job/target.hpp"
#include "mining_job/work_state.hpp"
#include "util/hex.hpp"

namespace {

cpu_miner::MiningJob make_fixture_job() {
   cpu_miner::MiningJob job;
   job.job_id = "69b23e1000005a88";
   job.prevhash =
      "60a003fb36548de6ed395d227308a62488b0d1c5000169d70000000000000000";
   job.coinb1 =
      "0100000001000000000000000000000000000000000000000000000000000000"
      "0000000000ffffffff3503335d0e0004609ebc69047e483d0d10";
   job.coinb2 =
      "0a636b706f6f6c0d2f42697441786520427272722fffffffff02f0c8a5120000"
      "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
      "00266a24aa21a9edae14689538cbacfe37f6e55e5acb11f6528600ede90d01f52"
      "328a7154468280700000000";
   job.merkle_branch = {
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
   job.version = "20000000";
   job.nbits = "1701f0cc";
   job.ntime = "69bc9e60";
   job.clean_jobs = false;
   return job;
}

cpu_miner::SubscriptionContext make_fixture_subscription() {
   cpu_miner::SubscriptionContext sub;
   sub.extranonce1 = "3e3eb26900000000";
   sub.extranonce2_size = 8U;
   return sub;
}

} // namespace

TEST_CASE("prepared work captures backend-neutral mining inputs",
          "[work_state]") {
   using namespace cpu_miner;

   const auto job = make_fixture_job();
   const auto sub = make_fixture_subscription();
   const auto prepared = prepare_work(job, sub, 0U);

   REQUIRE(prepared.extranonce2_hex == "0000000000000000");
   REQUIRE(prepared.coinbase.extranonce2_hex == "0000000000000000");
   REQUIRE(prepared.merkle_root_raw_hex ==
           "99da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c5ff10fec");
   REQUIRE(bytes_to_hex(prepared.prevhash_sha_input) ==
           "fb03a060e68d5436225d39ed24a60873c5d1b088d76901000000000000000000");
   REQUIRE(bytes_to_hex(prepared.merkle_root_sha_input) ==
           "99da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c5ff10fec");
}

TEST_CASE("prepared work can hash a nonce without runtime orchestration",
          "[work_state]") {
   using namespace cpu_miner;

   const auto job = make_fixture_job();
   const auto sub = make_fixture_subscription();
   const auto prepared = prepare_work(job, sub, 0U);

   const auto hash =
      hash_prepared_work_nonce(prepared, u32_from_hex_be("0525050b"));

   REQUIRE(bytes_to_hex(hash) ==
           "93dc89148fd563d2b8f734472172e4445b9ea5a5d1b25dc855124e76031f88ab");

   const auto share_target = share_target_from_difficulty(std::uint64_t{1});
   REQUIRE_FALSE(hash_meets_target(hash, share_target));
}

TEST_CASE("work_state remains a thin convenience wrapper", "[work_state]") {
   using namespace cpu_miner;

   const auto job = make_fixture_job();
   const auto sub = make_fixture_subscription();

   const auto prepared = prepare_work(job, sub, 0U);
   const auto work = work_state_from_prepared(prepared, 0U);

   REQUIRE(work.coinbase.extranonce2_hex == prepared.coinbase.extranonce2_hex);
   REQUIRE(work.merkle_root_raw_hex == prepared.merkle_root_raw_hex);
   REQUIRE(bytes_to_hex(work.prevhash_sha_input) ==
           bytes_to_hex(prepared.prevhash_sha_input));
   REQUIRE(bytes_to_hex(work.merkle_root_sha_input) ==
           bytes_to_hex(prepared.merkle_root_sha_input));
}
