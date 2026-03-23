// src/mining_job/backend.hpp

#ifndef CPU_MINER_MINING_JOB_BACKEND_HPP
#define CPU_MINER_MINING_JOB_BACKEND_HPP

#include <cstdint>
#include <functional>
#include <string_view>

#include "mining_job/scan.hpp"
#include "mining_job/work_state.hpp"
#include "util/uint256.hpp"

namespace cpu_miner {

struct BackendScanRequest {
   PreparedWork prepared;
   u256::uint256 network_target;
   u256::uint256 share_target;
   std::uint64_t nonce_begin{};
   std::uint64_t nonce_end{};
   std::uint64_t progress_interval{};
   ScanControl control{};
};

using BackendShareFoundCallback = std::function<void(const ShareCandidate&)>;

class HasherBackend {
 public:
   virtual ~HasherBackend() = default;

   [[nodiscard]] virtual std::string_view name() const noexcept = 0;

   [[nodiscard]] virtual ScanResult
   scan(const BackendScanRequest& request,
        BackendShareFoundCallback on_share_found) const = 0;
};

} // namespace cpu_miner

#endif
