// src/stratum_client/stratum_client.cpp

#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>
#include <variant>

#include "stratum_client/messages.hpp"
#include "stratum_client/stratum_client.hpp"

namespace cpu_miner {
namespace {

template<class... Ts>
struct Overload : Ts... {
   using Ts::operator()...;
};
template<class... Ts>
Overload(Ts...) -> Overload<Ts...>;

} // namespace

StratumClient::StratumClient(std::string host, std::string port)
   : host_(std::move(host))
   , port_(std::move(port))
   , resolver_(io_)
   , socket_(io_) {}

void StratumClient::connect() {
   const auto endpoints = resolver_.resolve(host_, port_);
   boost::asio::connect(socket_, endpoints);
}

void StratumClient::subscribe() {
   send_wire_message(to_wire_message(SubscribeRequest{}, next_id_++));
}

void StratumClient::suggest_difficulty(const double difficulty) {
   if (!(difficulty > 0.0) || !std::isfinite(difficulty)) {
      throw std::invalid_argument(
         "suggested difficulty must be finite and > 0");
   }

   send_wire_message(
      to_wire_message(SuggestDifficultyRequest{.difficulty = difficulty},
                      next_id_++));
}

void StratumClient::authorize(const std::string& user,
                              const std::string& password) {
   worker_name_ = user;
   send_wire_message(
      to_wire_message(AuthorizeRequest{.user = user, .password = password},
                      next_id_++));
}

void StratumClient::run_until_notify() {
   while (!saw_notify_) {
      const std::string line = read_line();
      if (line.empty()) continue;
      handle_message(line);
   }
}

const std::string& StratumClient::worker_name() const noexcept {
   return worker_name_;
}

const std::optional<SubscriptionContext>&
StratumClient::subscription() const noexcept {
   return subscription_;
}

const std::optional<MiningJob>& StratumClient::current_job() const noexcept {
   return current_job_;
}

double StratumClient::difficulty() const noexcept { return difficulty_; }

void StratumClient::send_wire_message(const std::string& wire) {
   std::string framed = wire;
   framed.push_back('\n');
   boost::asio::write(socket_, boost::asio::buffer(framed));
}

std::string StratumClient::read_line() {
   boost::asio::read_until(socket_, buffer_, '\n');

   std::istream input(&buffer_);
   std::string line;
   std::getline(input, line);

   if (!line.empty() && line.back() == '\r') {
      line.pop_back();
   }

   return line;
}

void StratumClient::handle_message(std::string_view line) {
   const auto parsed = parse_incoming_message(line);
   if (!parsed) {
      return;
   }

   std::visit(Overload{
                 [this](const UnknownMessage&) {},

                 [this](const SetDifficultyMessage& msg) {
                    difficulty_ = msg.difficulty;
                 },

                 [this](const NotifyMessage& msg) {
                    current_job_ = MiningJob{
                       .job_id = msg.job_id,
                       .prevhash = msg.prevhash,
                       .coinb1 = msg.coinb1,
                       .coinb2 = msg.coinb2,
                       .merkle_branch = msg.merkle_branch,
                       .version = msg.version,
                       .nbits = msg.nbits,
                       .ntime = msg.ntime,
                       .clean_jobs = msg.clean_jobs,
                    };
                    saw_notify_ = true;
                 },

                 [this](const SubscribeResponse& msg) {
                    subscription_ = SubscriptionContext{
                       .extranonce1 = msg.extranonce1,
                       .extranonce2_size = msg.extranonce2_size,
                    };
                 },

                 [this](const AuthorizeResponse&) {},

                 [this](const SubmitResponse&) {},
              },
              *parsed);
}

bool StratumClient::submit_share(const ShareSubmission& share) {
   if (worker_name_.empty()) {
      throw std::runtime_error("worker name is not set; authorize first");
   }

   const int submit_id = next_id_++;

   send_wire_message(to_wire_message(
      SubmitShareRequest{
         .worker_name = worker_name_,
         .job_id = share.job_id,
         .extranonce2_hex = share.extranonce2_hex,
         .ntime_hex = share.ntime_hex,
         .nonce_hex = share.nonce_hex,
      },
      submit_id));

   for (;;) {
      const std::string line = read_line();
      if (line.empty()) continue;

      const auto parsed = parse_incoming_message(line);
      if (!parsed) continue;

      bool keep_waiting = false;
      std::optional<bool> submit_result;

      std::visit(Overload{
                    [&](const UnknownMessage&) { keep_waiting = true; },

                    [&](const SetDifficultyMessage& msg) {
                       difficulty_ = msg.difficulty;
                       keep_waiting = true;
                    },

                    [&](const NotifyMessage& msg) {
                       current_job_ = MiningJob{
                          .job_id = msg.job_id,
                          .prevhash = msg.prevhash,
                          .coinb1 = msg.coinb1,
                          .coinb2 = msg.coinb2,
                          .merkle_branch = msg.merkle_branch,
                          .version = msg.version,
                          .nbits = msg.nbits,
                          .ntime = msg.ntime,
                          .clean_jobs = msg.clean_jobs,
                       };
                       keep_waiting = true;
                    },

                    [&](const SubscribeResponse& msg) {
                       subscription_ = SubscriptionContext{
                          .extranonce1 = msg.extranonce1,
                          .extranonce2_size = msg.extranonce2_size,
                       };
                       keep_waiting = true;
                    },

                    [&](const AuthorizeResponse&) { keep_waiting = true; },

                    [&](const SubmitResponse& msg) {
                       if (msg.id != submit_id) {
                          keep_waiting = true;
                          return;
                       }

                       if (msg.has_error) {
                          submit_result = false;
                          return;
                       }

                       submit_result = msg.accepted;
                    },
                 },
                 *parsed);

      if (submit_result.has_value()) {
         return *submit_result;
      }

      if (keep_waiting) {
         continue;
      }
   }
}

} // namespace cpu_miner

