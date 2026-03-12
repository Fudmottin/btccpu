// src/mining_job/share.hpp

#ifndef CPU_MINER_MINING_JOB_SHARE_HPP
#define CPU_MINER_MINING_JOB_SHARE_HPP

#include <cstdint>
#include <string>

namespace cpu_miner {

struct ShareSubmission {
   std::string job_id;
   std::string extranonce2_hex;
   std::string ntime_hex;
   std::string nonce_hex;
};

} // namespace cpu_miner

#endif

