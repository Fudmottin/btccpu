// src/mining_job/header.hpp

#ifndef CPU_MINER_MINING_JOB_HEADER_HPP
#define CPU_MINER_MINING_JOB_HEADER_HPP

#include <array>
#include <cstdint>
#include <string>

#include "sha256/sha256.hpp"

namespace cpu_miner {

using HeaderBytes = std::array<std::uint8_t, 80>;
using HashBytes = std::array<std::uint8_t, 32>;

struct HeaderTemplate {
   sha256::BlockWords block0{};
   sha256::BlockWords block1{};
   sha256::DigestWords midstate{};
};

HeaderBytes make_header_bytes(std::uint32_t version, const HashBytes& prevhash,
                              const HashBytes& merkle_root, std::uint32_t ntime,
                              std::uint32_t nbits, std::uint32_t nonce);

std::string header_hex(const HeaderBytes& header);

HeaderTemplate make_header_template(std::uint32_t version,
                                    const HashBytes& prevhash,
                                    const HashBytes& merkle_root,
                                    std::uint32_t ntime, std::uint32_t nbits,
                                    std::uint32_t nonce);

void set_header_nonce(HeaderTemplate& header, std::uint32_t nonce) noexcept;

sha256::DigestWords hash_header_template(const HeaderTemplate& header);

} // namespace cpu_miner

#endif

