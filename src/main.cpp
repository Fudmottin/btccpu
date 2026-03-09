// src/main.cpp

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

#include "mining_job/coinbase.hpp"
#include "mining_job/merkle.hpp"
#include "sha256/sha256.hpp"
#include "stratum_client/stratum_client.hpp"
#include "util/hex.hpp"

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
                   << coinbase_hex.substr(
                         coinbase_hex.size() > 192 ? coinbase_hex.size() - 192 : 0)
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

