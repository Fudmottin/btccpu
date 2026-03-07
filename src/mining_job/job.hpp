// src/mining_job/job.hpp
#ifndef CPU_MINER_MINING_JOB_JOB_HPP
#define CPU_MINER_MINING_JOB_JOB_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cpu_miner {

struct SubscriptionContext {
   std::string extranonce1;
   std::size_t extranonce2_size{};
};

struct MiningJob {
   std::string job_id;
   std::string prevhash;
   std::string coinb1;
   std::string coinb2;
   std::vector<std::string> merkle_branch;
   std::string version;
   std::string nbits;
   std::string ntime;
   bool clean_jobs{};
};

}  // namespace cpu_miner

#endif

