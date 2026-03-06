// src/stratum_client/messages.cpp
#include <iostream>
#include <stdexcept>

#include "stratum_client/messages.hpp"

namespace cpu_miner {
namespace {

const boost::json::array* find_params_array(const boost::json::object& obj) {
   const auto it = obj.find("params");
   if (it == obj.end() || !it->value().is_array()) return nullptr;
   return &it->value().as_array();
}

std::optional<std::string> json_string_at(const boost::json::array& a,
                                          const std::size_t index) {
   if (index >= a.size() || !a[index].is_string()) return std::nullopt;
   return std::string(a[index].as_string().c_str());
}

} // namespace

std::optional<SetDifficultyMessage>
parse_set_difficulty(const boost::json::object& obj) {
   const auto* params = find_params_array(obj);
   if (params == nullptr || params->empty()) return std::nullopt;

   const boost::json::value& v = (*params)[0];

   SetDifficultyMessage msg{};

   if (v.is_double()) {
      msg.difficulty = v.as_double();
      return msg;
   }

   if (v.is_int64()) {
      msg.difficulty = static_cast<double>(v.as_int64());
      return msg;
   }

   if (v.is_uint64()) {
      msg.difficulty = static_cast<double>(v.as_uint64());
      return msg;
   }

   return std::nullopt;
}

std::optional<NotifyMessage> parse_notify(const boost::json::object& obj) {
   const auto* params = find_params_array(obj);
   if (params == nullptr) return std::nullopt;
   if (params->size() < 9) return std::nullopt;

   NotifyMessage msg{};

   auto job_id = json_string_at(*params, 0);
   auto prevhash = json_string_at(*params, 1);
   auto coinb1 = json_string_at(*params, 2);
   auto coinb2 = json_string_at(*params, 3);
   auto version = json_string_at(*params, 5);
   auto nbits = json_string_at(*params, 6);
   auto ntime = json_string_at(*params, 7);

   if (!job_id || !prevhash || !coinb1 || !coinb2 || !version || !nbits ||
       !ntime) {
      return std::nullopt;
   }

   if (!(*params)[4].is_array()) return std::nullopt;
   if (!(*params)[8].is_bool()) return std::nullopt;

   msg.job_id = std::move(*job_id);
   msg.prevhash = std::move(*prevhash);
   msg.coinb1 = std::move(*coinb1);
   msg.coinb2 = std::move(*coinb2);
   msg.version = std::move(*version);
   msg.nbits = std::move(*nbits);
   msg.ntime = std::move(*ntime);
   msg.clean_jobs = (*params)[8].as_bool();

   const auto& branches = (*params)[4].as_array();
   msg.merkle_branch.reserve(branches.size());

   for (const auto& branch : branches) {
      if (!branch.is_string()) return std::nullopt;
      msg.merkle_branch.emplace_back(branch.as_string().c_str());
   }

   return msg;
}

void print_set_difficulty(const SetDifficultyMessage& msg) {
   std::cout << "set_difficulty:\n";
   std::cout << "  difficulty: " << msg.difficulty << '\n';
}

void print_notify_summary(const NotifyMessage& msg) {
   std::cout << "notify:\n";
   std::cout << "  job_id:        " << msg.job_id << '\n';
   std::cout << "  prevhash:      " << msg.prevhash << '\n';
   std::cout << "  coinb1 bytes:  " << (msg.coinb1.size() / 2) << '\n';
   std::cout << "  coinb2 bytes:  " << (msg.coinb2.size() / 2) << '\n';
   std::cout << "  branches:      " << msg.merkle_branch.size() << '\n';
   std::cout << "  version:       " << msg.version << '\n';
   std::cout << "  nbits:         " << msg.nbits << '\n';
   std::cout << "  ntime:         " << msg.ntime << '\n';
   std::cout << "  clean_jobs:    " << (msg.clean_jobs ? "true" : "false")
             << '\n';

   if (!msg.merkle_branch.empty()) {
      std::cout << "  first branch:  " << msg.merkle_branch.front() << '\n';
   }
}
} // namespace cpu_miner

