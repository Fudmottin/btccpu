// src/stratum_client/stratum_client.hpp

#ifndef CPU_MINER_STRATUM_CLIENT_STRATUM_CLIENT_HPP
#define CPU_MINER_STRATUM_CLIENT_STRATUM_CLIENT_HPP

#include <boost/asio.hpp>

#include <optional>
#include <string>
#include <string_view>

#include "mining_job/job.hpp"
#include "mining_job/share.hpp"

/*******************************************************************************
Purpose:
  Confine Stratum socket I/O to StratumClient and retain raw wire diagnostics
  for later reporting by the main thread.

Scope:
  - raw outgoing JSON line before write
  - raw incoming JSON line before parse
  - last raw mining.notify line
  - last parsed message summary

Requirements:
  - no printing from StratumClient
  - preserve typed-message flow from messages.*
  - keep changes small and non-disruptive

Do not:
  - move JSON parsing or serialization out of messages.*
  - refactor thread/event structure in this slice
  - add broad protocol redesign here

Stop when:
  - one rejected share can be traced to exact notify/request/response lines
    while preserving current architecture
*******************************************************************************/

namespace cpu_miner {

struct SubmitShareResult {
   bool accepted{};
   std::string error_text;
   std::string raw_request;
   std::string raw_response;
};

struct PollResult {
   bool got_message{};
   bool work_invalidated{};
};

class StratumClient {
 public:
   StratumClient(std::string host, std::string port);

   void connect();
   void subscribe();
   void suggest_difficulty(double difficulty);
   void authorize(const std::string& user, const std::string& password);

   void run_until_ready();
   [[nodiscard]] PollResult poll();
   [[nodiscard]] SubmitShareResult submit_share(const ShareSubmission& share);

   [[nodiscard]] const std::string& worker_name() const noexcept;
   [[nodiscard]] const std::optional<SubscriptionContext>&
   subscription() const noexcept;
   [[nodiscard]] const std::optional<MiningJob>& current_job() const noexcept;
   [[nodiscard]] double difficulty() const noexcept;

   [[nodiscard]] const std::string& last_raw_incoming() const noexcept;
   [[nodiscard]] const std::string& last_raw_outgoing() const noexcept;
   [[nodiscard]] const std::string& last_raw_notify() const noexcept;
   [[nodiscard]] const std::string& last_parsed_summary() const noexcept;

 private:
   void send_wire_message(const std::string& wire);
   std::string read_line();
   [[nodiscard]] PollResult handle_message(std::string_view line);
   [[nodiscard]] bool ready() const noexcept;

   std::string host_;
   std::string port_;

   boost::asio::io_context io_;
   boost::asio::ip::tcp::resolver resolver_;
   boost::asio::ip::tcp::socket socket_;
   boost::asio::streambuf buffer_;

   int next_id_{1};

   std::optional<SubscriptionContext> subscription_;
   std::optional<MiningJob> current_job_;
   double difficulty_{1.0};
   std::string worker_name_;

   std::string last_raw_incoming_;
   std::string last_raw_outgoing_;
   std::string last_raw_notify_;
   std::string last_parsed_summary_;
};

} // namespace cpu_miner

#endif

