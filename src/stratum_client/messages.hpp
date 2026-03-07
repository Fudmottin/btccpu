// src/stratum_client/messages.hpp
#ifndef CPU_MINER_STRATUM_CLIENT_MESSAGES_HPP
#define CPU_MINER_STRATUM_CLIENT_MESSAGES_HPP

#include <boost/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cpu_miner {

struct SetDifficultyMessage {
   double difficulty{};
};

struct NotifyMessage {
   std::string job_id;
   std::string prevhash;
   std::string coinb1;
   std::string coinb2;
   std::vector<std::string> merkle_branch;
   std::string version;
   std::string nbits;
   std::string ntime;
   bool clean_jobs{};
};

struct SubscribeResponse {
   std::string extranonce1;
   std::size_t extranonce2_size{};
};

std::optional<NotifyMessage> parse_notify(const boost::json::object& obj);

std::optional<SetDifficultyMessage>
parse_set_difficulty(const boost::json::object& obj);

std::optional<SubscribeResponse>
parse_subscribe_response(const boost::json::object& obj);

void print_set_difficulty(const SetDifficultyMessage& msg);

void print_notify_summary(const NotifyMessage& msg);

void print_subscribe_response(const SubscribeResponse& msg);

} // namespace cpu_miner

#endif

