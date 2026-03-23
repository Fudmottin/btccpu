#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "mining_job/header.hpp"
#include "mining_job/target.hpp"
#include "mining_job/work_state.hpp"
#include "sha256/sha256.hpp"
#include "util/hex.hpp"

namespace {

struct Fixture {
   cpu_miner::MiningJob job;
   cpu_miner::SubscriptionContext subscription;
   std::string extranonce2_hex;
   std::string nonce_hex;
   std::string expected_coinbase_hash_be;
   std::string expected_merkle_root_be;
   std::string expected_header_sha_input_hex;
   std::string expected_hash_raw_bytes;
   bool expected_meets_share_target;
   bool expected_meets_network_target;
};

Fixture make_rejected_fixture() {
   Fixture f;
   f.job.job_id = "69b23e1000005a88";
   f.job.prevhash =
      "60a003fb36548de6ed395d227308a62488b0d1c5000169d70000000000000000";
   f.job.coinb1 =
      "0100000001000000000000000000000000000000000000000000000000000000"
      "0000000000ffffffff3503335d0e0004609ebc69047e483d0d10";
   f.job.coinb2 =
      "0a636b706f6f6c0d2f42697441786520427272722fffffffff02f0c8a5120000"
      "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
      "00266a24aa21a9edae14689538cbacfe37f6e55e5acb11f6528600ede90d01f52"
      "328a7154468280700000000";
   f.job.merkle_branch = {
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
   f.job.version = "20000000";
   f.job.nbits = "1701f0cc";
   f.job.ntime = "69bc9e60";
   f.job.clean_jobs = false;

   f.subscription.extranonce1 = "3e3eb26900000000";
   f.subscription.extranonce2_size = 8U;

   f.extranonce2_hex = "0000000000000000";
   f.nonce_hex = "0525050b";
   f.expected_coinbase_hash_be =
      "b750de4f7cddd89844f5e188ea5c5aa71b55cdcf0b152eeff0844660f79eefa2";
   f.expected_merkle_root_be =
      "99da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c5ff10fec";
   f.expected_header_sha_input_hex =
      "00000020fb03a060e68d5436225d39ed24a60873c5d1b088d769010000000000"
      "0000000099da7a25d35032b52ed95376d944b4883bfbb8cec21775dd841f827c"
      "5ff10fec609ebc69ccf001170b052505";
   f.expected_hash_raw_bytes =
      "93dc89148fd563d2b8f734472172e4445b9ea5a5d1b25dc855124e76031f88ab";
   f.expected_meets_share_target = false;
   f.expected_meets_network_target = false;

   return f;
}

Fixture make_accepted_fixture() {
   Fixture f;
   f.job.job_id = "69b23e1000005c34";
   f.job.prevhash =
      "e51ad5fa5621c25d2acddbdf84d450603f465ea30000f5080000000000000000";
   f.job.coinb1 =
      "0100000001000000000000000000000000000000000000000000000000000000"
      "0000000000ffffffff3503405d0e0004f7cebc6904d997a02810";
   f.job.coinb2 =
      "0a636b706f6f6c0d2f42697441786520427272722fffffffff02288aa1120000"
      "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
      "00266a24aa21a9ed7f45ecf1c44f416bda0266b14e8f3e6c553963a34bee620a"
      "6d34656dee710bd000000000";
   f.job.merkle_branch = {
      "2cf3b8ed87f898203740953a940da8c0a6dfff5e6bd108c622f26a50a1658ffc",
      "7588f0078cd6500322384afe2a06bbdc1bfb3377534f973e0286877a5072e872",
      "de732123dfcb32c200d92937bae3b8d19ddf5490e420714bb3d14c1a91478d8f",
      "f0d85f993d2739e0fa0469b59fcda525eadf7b31c9b573bf7345b1f7bd7f1479",
      "57be3e820b0fd801617120f0098807b300bb810d5629cd3a73e7d29ab4cab121",
      "60469fdb5d1e072618c31b87aa323cadb355d516124bc7abe5765a53df1627bf",
      "c631ecc346327084d6d9ec866852c9fb7ecc76632c374dac3b79a40a68bae7b1",
   };
   f.job.version = "20000000";
   f.job.nbits = "1701f0cc";
   f.job.ntime = "69bccef7";
   f.job.clean_jobs = false;

   f.subscription.extranonce1 = "3f3eb26900000000";
   f.subscription.extranonce2_size = 8U;

   f.extranonce2_hex = "0000000000000000";
   f.nonce_hex = "00293f3b";
   f.expected_coinbase_hash_be =
      "393d2422f19699b4315270764c0402cb0ada6a3a510979a7f888f5ee5b2835b0";
   f.expected_merkle_root_be =
      "0f3f035de76c11fbd4597e034be737d6ff09432e036dcb24e93c4c5be5a0ef70";
   f.expected_header_sha_input_hex =
      "00000020fad51ae55dc22156dfdbcd2a6050d484a35e463f08f5000000000000"
      "000000000f3f035de76c11fbd4597e034be737d6ff09432e036dcb24e93c4c5b"
      "e5a0ef70f7cebc69ccf001173b3f2900";
   f.expected_hash_raw_bytes =
      "8bb6fe2d423e1030ca773a9e0f459f22bbbdb2f63bae0645e6118ca700000000";
   f.expected_meets_share_target = true;
   f.expected_meets_network_target = false;

   return f;
}

std::string reverse_hex_bytes(const std::string& hex) {
   const auto bytes = cpu_miner::hex_to_bytes(hex);
   std::vector<std::uint8_t> reversed(bytes.rbegin(), bytes.rend());
   return cpu_miner::bytes_to_hex(reversed);
}

void check_fixture(const Fixture& f) {
   using namespace cpu_miner;

   auto work = make_work_state(f.job, f.subscription, 0U);
   work = with_nonce(work, u32_from_hex_be(f.nonce_hex));

   const auto header =
      make_sha_input_header_bytes(u32_from_hex_be(f.job.version),
                                  work.prevhash_sha_input,
                                  work.merkle_root_sha_input,
                                  u32_from_hex_be(f.job.ntime),
                                  u32_from_hex_be(f.job.nbits), work.nonce);

   const auto hash_words = hash_header_template(work.header_template);
   const auto hash_bytes = sha256::digest_words_to_bytes_be(hash_words);

   REQUIRE(work.coinbase.extranonce2_hex == f.extranonce2_hex);
   REQUIRE(bytes_to_hex(work.coinbase.coinbase_hash) ==
           f.expected_coinbase_hash_be);
   REQUIRE(work.merkle_root_raw_hex == f.expected_merkle_root_be);
   REQUIRE(header_sha_input_hex(header) == f.expected_header_sha_input_hex);
   REQUIRE(bytes_to_hex(hash_bytes) == f.expected_hash_raw_bytes);

   const auto share_target = share_target_from_difficulty(std::uint64_t{1});
   const auto network_target =
      expand_compact_target(u32_from_hex_be(f.job.nbits));

   REQUIRE(hash_meets_target(hash_bytes, share_target) ==
           f.expected_meets_share_target);
   REQUIRE(hash_meets_target(hash_bytes, network_target) ==
           f.expected_meets_network_target);
}

} // namespace

TEST_CASE("rejected share fixture remains above target", "[regression]") {
   check_fixture(make_rejected_fixture());
}

TEST_CASE("accepted share fixture meets share target", "[regression]") {
   check_fixture(make_accepted_fixture());
}

TEST_CASE("accepted share display hash matches captured output",
          "[regression]") {
   const auto fixture = make_accepted_fixture();
   REQUIRE(reverse_hex_bytes(fixture.expected_hash_raw_bytes) ==
           "00000000a78c11e64506ae3bf6b2bdbb229f450f9e3a77ca30103e422dfeb68b");
}
