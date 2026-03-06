// src/main.cpp
#include <exception>
#include <iostream>
#include <string>

#include "stratum_client/stratum_client.hpp"

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

      std::cout << "Probe complete\n";
      return 0;
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

