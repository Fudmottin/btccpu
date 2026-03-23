// src/mining_job/cpu_backend.hpp

#ifndef CPU_MINER_MINING_JOB_CPU_BACKEND_HPP
#define CPU_MINER_MINING_JOB_CPU_BACKEND_HPP

#include <string_view>

#include "mining_job/backend.hpp"

namespace cpu_miner {

class CpuHasherBackend final : public HasherBackend {
 public:
   [[nodiscard]] std::string_view name() const noexcept override;

   [[nodiscard]] ScanResult
   scan(const BackendScanRequest& request,
        BackendShareFoundCallback on_share_found) const override;
};

} // namespace cpu_miner

#endif
