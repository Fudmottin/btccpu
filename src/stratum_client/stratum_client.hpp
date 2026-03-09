// src/stratum_client/stratum_client.hpp

#ifndef CPU_MINER_STRATUM_CLIENT_STRATUM_CLIENT_HPP
#define CPU_MINER_STRATUM_CLIENT_STRATUM_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>

#include "mining_job/job.hpp"

namespace cpu_miner {

class StratumClient {
 public:
   StratumClient(std::string host, std::string port);

   void connect();
   void subscribe();
   void authorize(const std::string& user, const std::string& password);
   void run_until_notify();

   [[nodiscard]] const std::optional<SubscriptionContext>&
   subscription() const noexcept;
   [[nodiscard]] const std::optional<MiningJob>& current_job() const noexcept;
   [[nodiscard]] double difficulty() const noexcept;

 private:
   void send_json(const boost::json::object& message);
   std::string read_line();
   void handle_message(std::string_view line);
   void print_json_pretty(const boost::json::value& value) const;

   std::string host_;
   std::string port_;

   boost::asio::io_context io_;
   boost::asio::ip::tcp::resolver resolver_;
   boost::asio::ip::tcp::socket socket_;
   boost::asio::streambuf buffer_;

   int next_id_{1};
   bool saw_notify_{false};

   std::optional<SubscriptionContext> subscription_;
   std::optional<MiningJob> current_job_;
   double difficulty_{1.0};
};

} // namespace cpu_miner

#endif

