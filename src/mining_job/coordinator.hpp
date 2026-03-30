// src/mining_job/coordinator.hpp

#ifndef CPU_MINER_MINING_JOB_COORDINATOR_HPP
#define CPU_MINER_MINING_JOB_COORDINATOR_HPP

#include <cstdint>
#include <functional>
#include <optional>

#include "mining_job/backend.hpp"
#include "mining_job/work_state.hpp"
#include "util/uint256.hpp"

namespace cpu_miner {

using CoordinatorShareFoundCallback =
   std::function<void(const ShareSubmission&, const ShareCandidate&)>;

class MiningCoordinator {
 public:
   explicit MiningCoordinator(HasherBackend& backend);

   void set_job(const MiningJob& job, const SubscriptionContext& subscription,
                std::uint64_t extranonce2_counter = 0);

   [[nodiscard]] std::uint64_t generation() const noexcept;

   void on_share_found(CoordinatorShareFoundCallback cb);

   [[nodiscard]] ScanResult
   scan_range(std::uint64_t nonce_begin, std::uint64_t nonce_end,
              const u256::uint256& network_target,
              const u256::uint256& share_target,
              std::uint64_t progress_interval = 0) const;

 private:
   HasherBackend* backend_{};
   std::optional<PreparedWork> prepared_;
   std::uint64_t generation_{0};
   CoordinatorShareFoundCallback on_share_found_{};
};

} // namespace cpu_miner

#endif

