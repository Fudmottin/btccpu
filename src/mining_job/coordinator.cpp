// src/mining_job/coordinator.cpp

#include <stdexcept>

#include "mining_job/coordinator.hpp"

namespace cpu_miner {

MiningCoordinator::MiningCoordinator(HasherBackend& backend)
   : backend_(&backend) {}

void MiningCoordinator::set_job(const MiningJob& job,
                                const SubscriptionContext& subscription,
                                std::uint64_t extranonce2_counter) {
   prepared_ = prepare_work(job, subscription, extranonce2_counter);
   ++generation_;
}

std::uint64_t MiningCoordinator::generation() const noexcept {
   return generation_;
}

void MiningCoordinator::on_share_found(CoordinatorShareFoundCallback cb) {
   on_share_found_ = std::move(cb);
}

CoordinatorResult MiningCoordinator::scan_range(
   std::uint64_t nonce_begin, std::uint64_t nonce_end,
   const u256::uint256& network_target, const u256::uint256& share_target,
   std::uint64_t progress_interval) const {

   if (!prepared_) {
      throw std::runtime_error("scan_range called without prepared work");
   }

   const auto request_generation = generation_;

   BackendScanRequest request{
      .prepared = *prepared_,
      .network_target = network_target,
      .share_target = share_target,
      .nonce_begin = nonce_begin,
      .nonce_end = nonce_end,
      .progress_interval = progress_interval,
      .control = {},
   };

   CoordinatorResult result{};

   const auto backend_result =
      backend_->scan(request, [&](const ShareCandidate& candidate) {
         if (request_generation != generation_) {
            return; // stale
         }

         ++result.shares_found;
         if (candidate.is_block_candidate) {
            ++result.blocks_found;
         }

         if (on_share_found_) {
            const auto submission =
               make_share_submission(request.prepared, candidate.nonce);

            on_share_found_(submission, candidate);
         }
      });

   result.hashes_done = backend_result.hashes_done;

   return result;
}

} // namespace cpu_miner

