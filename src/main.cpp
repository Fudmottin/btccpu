// src/main.cpp

#include <algorithm>
#include <array>
#include <exception>
#include <iostream>
#include <string>

#include "mining_job/coinbase.hpp"
#include "mining_job/header.hpp"
#include "mining_job/merkle.hpp"
#include "sha256/sha256.hpp"
#include "stratum_client/stratum_client.hpp"
#include "util/hex.hpp"

namespace {

std::array<std::uint8_t, 32> hash32_from_hex(std::string_view hex) {
   const auto bytes = cpu_miner::hex_to_bytes(hex);
   if (bytes.size() != 32U) {
      throw std::invalid_argument("expected 32-byte hash hex");
   }

   std::array<std::uint8_t, 32> out{};
   for (std::size_t i = 0; i < out.size(); ++i) {
      out[i] = bytes[i];
   }
   return out;
}

void reverse_bytes(std::span<std::uint8_t> bytes) {
   std::reverse(bytes.begin(), bytes.end());
}
} // namespace

int main(int argc, char* argv[]) {
   try {
      const std::string host = (argc > 1) ? argv[1] : "192.168.0.104";
      const std::string port = (argc > 2) ? argv[2] : "3333";
      const std::string user =
         (argc > 3) ? argv[3] : "bc1qyourwalletaddresshere.cpu-miner";
      const std::string password = (argc > 4) ? argv[4] : "x";

      cpu_miner::StratumClient client(host, port);
      client.connect();
      client.subscribe();
      client.authorize(user, password);
      client.run_until_notify();

      if (client.subscription() && client.current_job()) {
         const auto& sub = *client.subscription();
         const auto& job = *client.current_job();

         const std::string extranonce2 =
            cpu_miner::extranonce2_from_counter(0, sub.extranonce2_size);

         const std::string coinbase_hex =
            cpu_miner::make_coinbase_hex(job, sub, extranonce2);

         std::cout << "coinbase demo:\n";
         std::cout << "  extranonce2: " << extranonce2 << '\n';
         std::cout << "  coinbase hex chars: " << coinbase_hex.size() << '\n';
         std::cout << "  coinbase bytes: " << (coinbase_hex.size() / 2U)
                   << '\n';

         const std::size_t preview_len =
            std::min<std::size_t>(coinbase_hex.size(), 64U);
         std::cout << "  coinbase prefix: "
                   << coinbase_hex.substr(0, preview_len) << '\n';
         std::cout << "  coinbase suffix: "
                   << coinbase_hex.substr(coinbase_hex.size() > 192
                                             ? coinbase_hex.size() - 192
                                             : 0)
                   << '\n';

         const auto coinbase_bytes = cpu_miner::hex_to_bytes(coinbase_hex);
         const auto coinbase_hash_words =
            cpu_miner::sha256::dbl_sha256_words(coinbase_bytes);
         const auto coinbase_hash_bytes =
            cpu_miner::sha256::digest_words_to_bytes_be(coinbase_hash_words);
         const std::string coinbase_hash_hex =
            cpu_miner::bytes_to_hex(coinbase_hash_bytes);

         std::cout << "  coinbase_hash: " << coinbase_hash_hex << '\n';

         const auto merkle_root =
            cpu_miner::merkle_root_hex(coinbase_hash_bytes, job.merkle_branch);
         std::cout << "  merkle_root: " << merkle_root << '\n';

         const std::uint32_t version = cpu_miner::u32_from_hex_be(job.version);
         const std::uint32_t ntime = cpu_miner::u32_from_hex_be(job.ntime);
         const std::uint32_t nbits = cpu_miner::u32_from_hex_be(job.nbits);
         const std::uint32_t nonce = 0U;

         auto prevhash = hash32_from_hex(job.prevhash);
         auto merkle_root_bytes = hash32_from_hex(merkle_root);

         reverse_bytes(prevhash);
         reverse_bytes(merkle_root_bytes);

         const auto header =
            cpu_miner::make_header_bytes(version, prevhash, merkle_root_bytes,
                                         ntime, nbits, nonce);

         const std::string header_hex = cpu_miner::header_hex(header);
         const auto header_hash_words =
            cpu_miner::sha256::dbl_sha256_words(header);
         const auto header_hash_bytes =
            cpu_miner::sha256::digest_words_to_bytes_be(header_hash_words);
         const std::string header_hash_hex =
            cpu_miner::bytes_to_hex(header_hash_bytes);

         std::cout << "  header_hex: " << header_hex << '\n';
         std::cout << "  header_hash_nonce0: " << header_hash_hex << '\n';

         auto header_template =
            cpu_miner::make_header_template(version, prevhash,
                                            merkle_root_bytes, ntime, nbits,
                                            nonce);

         cpu_miner::set_header_nonce(header_template, nonce);

         const auto template_hash_words =
            cpu_miner::hash_header_template(header_template);
         const auto template_hash_bytes =
            cpu_miner::sha256::digest_words_to_bytes_be(template_hash_words);
         const std::string template_hash_hex =
            cpu_miner::bytes_to_hex(template_hash_bytes);

         std::cout << "  template_hash_nonce0: " << template_hash_hex << '\n';
      } else {
         std::cout << "coinbase demo skipped: missing subscription or job\n";
      }

      std::cout << "Probe complete\n";
      return 0;
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

