// src/stratum_client/stratum_client.cpp
#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <cmath>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "stratum_client/messages.hpp"
#include "stratum_client/stratum_client.hpp"

namespace cpu_miner {

StratumClient::StratumClient(std::string host, std::string port)
   : host_(std::move(host))
   , port_(std::move(port))
   , resolver_(io_)
   , socket_(io_) {}

void StratumClient::connect() {
   std::cout << "Connecting to " << host_ << ':' << port_ << '\n';
   const auto endpoints = resolver_.resolve(host_, port_);
   boost::asio::connect(socket_, endpoints);
   std::cout << "Connected\n";
}

void StratumClient::subscribe() {
   boost::json::object message;
   message["id"] = next_id_++;
   message["method"] = "mining.subscribe";
   message["params"] = boost::json::array{};

   std::cout << "-> mining.subscribe\n";
   send_json(message);
}

void StratumClient::suggest_difficulty(double difficulty) {
   if (!(difficulty > 0.0) || !std::isfinite(difficulty)) {
      throw std::invalid_argument(
         "suggested difficulty must be finite and > 0");
   }

   boost::json::object message;
   message["id"] = next_id_++;
   message["method"] = "mining.suggest_difficulty";
   message["params"] = boost::json::array{difficulty};

   std::cout << "-> mining.suggest_difficulty difficulty=" << difficulty
             << '\n';
   send_json(message);
}

void StratumClient::authorize(const std::string& user,
                              const std::string& password) {
   boost::json::object message;
   message["id"] = next_id_++;
   message["method"] = "mining.authorize";
   message["params"] = boost::json::array{user, password};
   worker_name_ = user;

   std::cout << "-> mining.authorize user=" << user << '\n';
   send_json(message);
}

void StratumClient::run_until_notify() {
   while (!saw_notify_) {
      const std::string line = read_line();
      if (line.empty()) continue;
      handle_message(line);
   }
}

const std::optional<SubscriptionContext>&
StratumClient::subscription() const noexcept {
   return subscription_;
}

const std::optional<MiningJob>& StratumClient::current_job() const noexcept {
   return current_job_;
}

double StratumClient::difficulty() const noexcept { return difficulty_; }

void StratumClient::send_json(const boost::json::object& message) {
   std::string wire = boost::json::serialize(message);
   wire.push_back('\n');
   boost::asio::write(socket_, boost::asio::buffer(wire));
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
   std::cout << "<- " << line << '\n';

   boost::system::error_code ec;
   const boost::json::value value = boost::json::parse(line, ec);
   if (ec) {
      std::cerr << "JSON parse error: " << ec.message() << '\n';
      return;
   }

   if (!value.is_object()) {
      std::cerr << "Ignoring non-object JSON message\n";
      return;
   }

   const auto& obj = value.as_object();

   const auto method_it = obj.find("method");
   if (method_it != obj.end() && method_it->value().is_string()) {
      const std::string method =
         std::string(method_it->value().as_string().c_str());

      if (method == "mining.set_difficulty") {
         const auto msg = parse_set_difficulty(obj);
         if (!msg) {
            std::cerr << "Failed to parse mining.set_difficulty\n";
            print_json_pretty(value);
            return;
         }

         difficulty_ = msg->difficulty;
         return;
      }

      if (method == "mining.notify") {
         const auto msg = parse_notify(obj);
         if (!msg) {
            std::cerr << "Failed to parse mining.notify\n";
            print_json_pretty(value);
            return;
         }

         current_job_ = MiningJob{.job_id = msg->job_id,
                                  .prevhash = msg->prevhash,
                                  .coinb1 = msg->coinb1,
                                  .coinb2 = msg->coinb2,
                                  .merkle_branch = msg->merkle_branch,
                                  .version = msg->version,
                                  .nbits = msg->nbits,
                                  .ntime = msg->ntime,
                                  .clean_jobs = msg->clean_jobs};

         if (subscription_) {
            std::cout << "subscription cached: extranonce1="
                      << subscription_->extranonce1 << '\n';
         }
         if (current_job_) {
            std::cout << "job cached: " << current_job_->job_id << '\n';
         }

         print_notify_summary(*msg);
         saw_notify_ = true;
         return;
      }

      std::cout << "Received method: " << method << '\n';
      print_json_pretty(value);
      return;
   }

   const auto id_it = obj.find("id");
   if (id_it != obj.end()) {
      const auto subscribe = parse_subscribe_response(obj);
      if (subscribe) {
         subscription_ =
            SubscriptionContext{.extranonce1 = subscribe->extranonce1,
                                .extranonce2_size =
                                   subscribe->extranonce2_size};

         print_subscribe_response(*subscribe);
         return;
      }

      std::cout << "Received response message\n";
      print_json_pretty(value);
      return;
   }

   std::cout << "Received unclassified message\n";
   print_json_pretty(value);
}

bool StratumClient::submit_share(const ShareSubmission& share) {
   if (worker_name_.empty()) {
      throw std::runtime_error("worker name is not set; authorize first");
   }

   boost::json::object message;
   const int submit_id = next_id_++;
   message["id"] = submit_id;
   message["method"] = "mining.submit";
   message["params"] = boost::json::array{
      worker_name_,    share.job_id,    share.extranonce2_hex,
      share.ntime_hex, share.nonce_hex,
   };

   std::cout << "-> mining.submit"
             << " worker=" << worker_name_ << " job_id=" << share.job_id
             << " extranonce2=" << share.extranonce2_hex
             << " ntime=" << share.ntime_hex << " nonce=" << share.nonce_hex
             << '\n';

   send_json(message);

   for (;;) {
      const std::string line = read_line();
      if (line.empty()) continue;

      std::cout << "<- " << line << '\n';

      boost::system::error_code ec;
      const boost::json::value value = boost::json::parse(line, ec);
      if (ec) {
         std::cerr << "JSON parse error: " << ec.message() << '\n';
         continue;
      }

      if (!value.is_object()) {
         std::cerr << "Ignoring non-object JSON message\n";
         continue;
      }

      const auto& obj = value.as_object();

      const auto method_it = obj.find("method");
      if (method_it != obj.end() && method_it->value().is_string()) {
         handle_message(line);
         continue;
      }

      const auto id_it = obj.find("id");
      if (id_it == obj.end() || !id_it->value().is_int64()) {
         std::cout << "Received unclassified submit response\n";
         print_json_pretty(value);
         continue;
      }

      if (id_it->value().as_int64() != submit_id) {
         std::cout << "Received response for different request id\n";
         print_json_pretty(value);
         continue;
      }

      const auto error_it = obj.find("error");
      if (error_it != obj.end() && !error_it->value().is_null()) {
         std::cout << "share rejected by pool\n";
         print_json_pretty(value);
         return false;
      }

      const auto result_it = obj.find("result");
      if (result_it != obj.end() && result_it->value().is_bool()) {
         const bool accepted = result_it->value().as_bool();
         std::cout << "share submit result: "
                   << (accepted ? "accepted" : "rejected") << '\n';
         return accepted;
      }

      std::cout << "unexpected submit response shape\n";
      print_json_pretty(value);
      return false;
   }
}

void StratumClient::print_json_pretty(const boost::json::value& value) const {
   std::cout << boost::json::serialize(value) << '\n';
}

} // namespace cpu_miner

