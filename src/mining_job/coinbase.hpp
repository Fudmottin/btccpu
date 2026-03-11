// src/mining_job/coinbase.hpp

#ifndef CPU_MINER_MINING_JOB_COINBASE_HPP
#define CPU_MINER_MINING_JOB_COINBASE_HPP

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "mining_job/job.hpp"
#include "sha256/sha256.hpp"

namespace cpu_miner {

struct ScriptSigSummary {
   bool has_block_height{false};
   std::uint32_t block_height{};
   std::vector<std::string> ascii_runs;
};

struct CoinbaseOutputDecoded {
   std::uint64_t value_sats{};
   std::vector<std::uint8_t> script_pubkey;
   std::string script_type;
   std::string address;
};

struct CoinbaseDecoded {
   std::uint32_t version{};
   bool has_witness_marker{false};
   std::vector<std::uint8_t> prevout_hash;
   std::uint32_t prevout_index{};
   std::vector<std::uint8_t> script_sig;
   std::uint32_t sequence{};
   std::vector<CoinbaseOutputDecoded> outputs;
   std::uint32_t locktime{};
};

struct CoinbaseBuild {
   std::string extranonce2_hex;
   std::string coinbase_hex;
   std::vector<std::uint8_t> coinbase_bytes;
   sha256::DigestBytes coinbase_hash;
};

std::string extranonce2_from_counter(std::uint64_t value,
                                     std::size_t size_bytes);

std::string make_coinbase_hex(const MiningJob& job,
                              const SubscriptionContext& sub,
                              const std::string& extranonce2_hex);

CoinbaseBuild build_coinbase(const MiningJob& job,
                             const SubscriptionContext& sub,
                             const std::string& extranonce2_hex);

std::string
decode_p2wpkh_address(const std::vector<std::uint8_t>& script_pubkey);
std::string
decode_p2wsh_address(const std::vector<std::uint8_t>& script_pubkey);

ScriptSigSummary summarize_script_sig(const std::vector<std::uint8_t>& script_sig);

CoinbaseDecoded decode_coinbase(const std::vector<std::uint8_t>& tx_bytes);

void print_coinbase_decoded(const CoinbaseDecoded& decoded, std::ostream& os);

} // namespace cpu_miner

#endif

