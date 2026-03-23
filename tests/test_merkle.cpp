#include <catch2/catch_test_macros.hpp>
#include <string>

#include "mining_job/coinbase.hpp"
#include "mining_job/job.hpp"
#include "mining_job/merkle.hpp"
#include "util/hex.hpp"

namespace {

cpu_miner::MiningJob make_rejected_job() {
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

cpu_miner::SubscriptionContext make_rejected_subscription() {
   cpu_miner::SubscriptionContext sub;
   sub.extranonce1 = "3e3eb26900000000";
   sub.extranonce2_size = 8U;
   return sub;
}

} // namespace

TEST_CASE("coinbase hash and merkle root match rejected fixture", "[merkle]") {
   using namespace cpu_miner;

   const auto job = make_rejected_job();
   const auto sub = make_rejected_subscription();

   const auto coinbase = build_coinbase(job, sub, "0000000000000000");
   const auto coinbase_hash_hex = bytes_to_hex(coinbase.coinbase_hash);
   const auto merkle_root_hex =
      merkle_root_raw_hex(coinbase.coinbase_hash, job.merkle_branch);

   REQUIRE(coinbase.coinbase_hex == job.coinb1 + sub.extranonce1 +
                                       std::string("0000000000000000") +
                                       job.coinb2);
   REQUIRE(coinbase_hash_hex ==
           "b750de4f7cddd89844f5e188ea5c5aa71b55cdcf0b152eeff0844660f79eefa2");
   REQUIRE(merkle_root_hex ==
           "99da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c5ff10fec");
}
