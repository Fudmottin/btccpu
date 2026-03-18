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

PollResult
apply_parsed_message(const IncomingMessage& parsed,
                     std::optional<SubscriptionContext>& subscription,
                     std::optional<MiningJob>& current_job,
                     double& difficulty) {
   PollResult result{};

   std::visit(Overload{
                 [&](const UnknownMessage&) { result.got_message = true; },

                 [&](const SetDifficultyMessage& msg) {
                    result.got_message = true;
                    if (difficulty != msg.difficulty) {
                       difficulty = msg.difficulty;
                       result.work_invalidated = true;
                    }
                 },

                 [&](const NotifyMessage& msg) {
                    result.got_message = true;
                    current_job = MiningJob{
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
                    result.work_invalidated = true;
                 },

                 [&](const SubscribeResponse& msg) {
                    result.got_message = true;
                    subscription = SubscriptionContext{
                       .extranonce1 = msg.extranonce1,
                       .extranonce2_size = msg.extranonce2_size,
                    };
                 },

                 [&](const AuthorizeResponse&) { result.got_message = true; },

                 [&](const SubmitResponse&) { result.got_message = true; },
              },
              parsed);

   return result;
}

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

void StratumClient::run_until_ready() {
   while (!ready()) {
      const std::string line = read_line();
      if (line.empty()) continue;
      (void)handle_message(line);
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

const std::string& StratumClient::last_raw_incoming() const noexcept {
   return last_raw_incoming_;
}

const std::string& StratumClient::last_raw_outgoing() const noexcept {
   return last_raw_outgoing_;
}

const std::string& StratumClient::last_raw_notify() const noexcept {
   return last_raw_notify_;
}

const std::string& StratumClient::last_parsed_summary() const noexcept {
   return last_parsed_summary_;
}

void StratumClient::send_wire_message(const std::string& wire) {
   last_raw_outgoing_ = wire;

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

   last_raw_incoming_ = line;
   return line;
}

PollResult StratumClient::handle_message(std::string_view line) {
   const auto parsed = parse_incoming_message(line);
   if (!parsed) {
      last_parsed_summary_.clear();
      return {};
   }

   last_parsed_summary_ = debug_summary(*parsed);

   if (std::holds_alternative<NotifyMessage>(*parsed)) {
      last_raw_notify_ = std::string(line);
   }

   return apply_parsed_message(*parsed, subscription_, current_job_,
                               difficulty_);
}

bool StratumClient::ready() const noexcept {
   return subscription_.has_value() && current_job_.has_value();
}

PollResult StratumClient::poll() {
   boost::system::error_code ec;
   const auto available = socket_.available(ec);
   if (ec || available == 0U) {
      return {};
   }

   const std::string line = read_line();
   if (line.empty()) {
      return {};
   }

   return handle_message(line);
}

SubmitShareResult StratumClient::submit_share(const ShareSubmission& share) {
   if (worker_name_.empty()) {
      throw std::runtime_error("worker name is not set; authorize first");
   }

   const int submit_id = next_id_++;

   const SubmitShareRequest request{
      .worker_name = worker_name_,
      .job_id = share.job_id,
      .extranonce2_hex = share.extranonce2_hex,
      .ntime_hex = share.ntime_hex,
      .nonce_hex = share.nonce_hex,
   };

   const std::string wire = to_wire_message(request, submit_id);
   send_wire_message(wire);

   for (;;) {
      const std::string line = read_line();
      if (line.empty()) continue;

      const auto parsed = parse_incoming_message(line);
      if (!parsed) continue;

      last_parsed_summary_ = debug_summary(*parsed);

      if (std::holds_alternative<NotifyMessage>(*parsed)) {
         last_raw_notify_ = line;
      }

      if (const auto* submit = std::get_if<SubmitResponse>(&*parsed)) {
         if (submit->id != submit_id) {
            (void)apply_parsed_message(*parsed, subscription_, current_job_,
                                       difficulty_);
            continue;
         }

         SubmitShareResult result;
         result.accepted = submit->accepted;
         result.error_text = submit->error_text;
         result.raw_request = wire;
         result.raw_response = line;
         return result;
      }

      (void)apply_parsed_message(*parsed, subscription_, current_job_,
                                 difficulty_);
   }
}

} // namespace cpu_miner

