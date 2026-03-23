// src/mining_job/cpu_backend.cpp

#include "mining_job/cpu_backend.hpp"

#include <cstdint>
#include <string_view>

namespace cpu_miner {

std::string_view CpuHasherBackend::name() const noexcept { return "cpu"; }

ScanResult
CpuHasherBackend::scan(const BackendScanRequest& request,
                       BackendShareFoundCallback on_share_found) const {
   auto work =
      work_state_from_prepared(request.prepared,
                               static_cast<std::uint32_t>(request.nonce_begin));
   const auto base_work = work_state_from_prepared(request.prepared, 0U);

   return scan_nonce_range(work, request.network_target, request.share_target,
                           request.nonce_begin, request.nonce_end,
                           request.progress_interval, request.control,
                           [&](std::uint32_t nonce,
                               const sha256::DigestBytes& hash,
                               bool is_block_candidate) {
                              if (!on_share_found) return;

                              ShareCandidate candidate;
                              candidate.work = with_nonce(base_work, nonce);
                              candidate.nonce = nonce;
                              candidate.hash = hash;
                              candidate.is_block_candidate = is_block_candidate;
                              candidate.generation =
                                 request.control.expected_generation;
                              on_share_found(candidate);
                           });
}

} // namespace cpu_miner
