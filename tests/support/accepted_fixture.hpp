#ifndef CPU_MINER_TESTS_SUPPORT_ACCEPTED_FIXTURE_HPP
#define CPU_MINER_TESTS_SUPPORT_ACCEPTED_FIXTURE_HPP

#include "mining_job/job.hpp"

namespace cpu_miner::test_support {

inline MiningJob make_accepted_job() {
   MiningJob job;
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

inline SubscriptionContext make_accepted_subscription() {
   SubscriptionContext sub;
   sub.extranonce1 = "3f3eb26900000000";
   sub.extranonce2_size = 8U;
   return sub;
}

} // namespace cpu_miner::test_support

#endif
