// src/stratum_client/messages.cpp

#include <boost/json.hpp>

#include <limits>
#include <sstream>
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

std::optional<int> parse_message_id(const boost::json::object& obj) {
   const auto id_it = obj.find("id");
   if (id_it == obj.end()) return std::nullopt;

   const auto& id_value = id_it->value();
   if (!id_value.is_int64()) return std::nullopt;

   const auto id64 = id_value.as_int64();
   if (id64 < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
       id64 > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
      return std::nullopt;
   }

   return static_cast<int>(id64);
}

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

std::optional<SubscribeResponse>
parse_subscribe_response(const boost::json::object& obj) {
   const auto id = parse_message_id(obj);
   if (!id) return std::nullopt;

   const auto result_it = obj.find("result");
   if (result_it == obj.end() || !result_it->value().is_array()) {
      return std::nullopt;
   }

   const auto& result = result_it->value().as_array();
   if (result.size() < 3) return std::nullopt;
   if (!result[1].is_string()) return std::nullopt;

   const boost::json::value& extranonce2_size_value = result[2];

   std::size_t extranonce2_size{};
   if (extranonce2_size_value.is_uint64()) {
      extranonce2_size =
         static_cast<std::size_t>(extranonce2_size_value.as_uint64());
   } else if (extranonce2_size_value.is_int64()) {
      const auto n = extranonce2_size_value.as_int64();
      if (n < 0) return std::nullopt;
      extranonce2_size = static_cast<std::size_t>(n);
   } else {
      return std::nullopt;
   }

   SubscribeResponse msg{};
   msg.id = *id;
   msg.extranonce1 = std::string(result[1].as_string().c_str());
   msg.extranonce2_size = extranonce2_size;
   return msg;
}

std::optional<AuthorizeResponse>
parse_authorize_response(const boost::json::object& obj) {
   const auto id = parse_message_id(obj);
   if (!id) return std::nullopt;

   const auto result_it = obj.find("result");
   if (result_it == obj.end() || !result_it->value().is_bool()) {
      return std::nullopt;
   }

   AuthorizeResponse msg{};
   msg.id = *id;
   msg.accepted = result_it->value().as_bool();
   return msg;
}

std::optional<SubmitResponse>
parse_submit_response(const boost::json::object& obj) {
   const auto id = parse_message_id(obj);
   if (!id) return std::nullopt;

   SubmitResponse msg{};
   msg.id = *id;

   const auto result_it = obj.find("result");
   if (result_it != obj.end()) {
      const auto& result = result_it->value();
      if (result.is_bool()) {
         msg.accepted = result.as_bool();
      } else if (result.is_null()) {
         msg.accepted = false;
      } else {
         return std::nullopt;
      }
   } else {
      msg.accepted = false;
   }

   const auto error_it = obj.find("error");
   if (error_it != obj.end() && !error_it->value().is_null()) {
      msg.has_error = true;
      msg.error_text = boost::json::serialize(error_it->value());
   }

   return msg;
}

std::optional<IncomingMessage>
parse_method_message(const boost::json::object& obj) {
   const auto method_it = obj.find("method");
   if (method_it == obj.end() || !method_it->value().is_string()) {
      return std::nullopt;
   }

   const std::string method =
      std::string(method_it->value().as_string().c_str());

   if (method == "mining.set_difficulty") {
      if (const auto msg = parse_set_difficulty(obj)) return *msg;
      return std::nullopt;
   }

   if (method == "mining.notify") {
      if (const auto msg = parse_notify(obj)) return *msg;
      return std::nullopt;
   }

   return UnknownMessage{.raw = boost::json::serialize(obj)};
}

} // namespace

std::string to_wire_message(const SubscribeRequest&, const int id) {
   boost::json::object message;
   message["id"] = id;
   message["method"] = "mining.subscribe";
   message["params"] = boost::json::array{};
   return boost::json::serialize(message);
}

std::string to_wire_message(const SuggestDifficultyRequest& request,
                            const int id) {
   boost::json::object message;
   message["id"] = id;
   message["method"] = "mining.suggest_difficulty";
   message["params"] = boost::json::array{request.difficulty};
   return boost::json::serialize(message);
}

std::string to_wire_message(const AuthorizeRequest& request, const int id) {
   boost::json::object message;
   message["id"] = id;
   message["method"] = "mining.authorize";
   message["params"] = boost::json::array{request.user, request.password};
   return boost::json::serialize(message);
}

std::string to_wire_message(const SubmitShareRequest& request, const int id) {
   boost::json::object message;
   message["id"] = id;
   message["method"] = "mining.submit";
   message["params"] = boost::json::array{
      request.worker_name, request.job_id,    request.extranonce2_hex,
      request.ntime_hex,   request.nonce_hex,
   };
   return boost::json::serialize(message);
}

std::optional<IncomingMessage> parse_incoming_message(std::string_view line) {
   boost::system::error_code ec;
   const boost::json::value value = boost::json::parse(line, ec);
   if (ec) return std::nullopt;
   if (!value.is_object()) return std::nullopt;

   const auto& obj = value.as_object();

   if (const auto msg = parse_method_message(obj)) {
      return msg;
   }

   if (const auto msg = parse_subscribe_response(obj)) {
      return *msg;
   }

   if (const auto msg = parse_submit_response(obj)) {
      return *msg;
   }

   if (const auto msg = parse_authorize_response(obj)) {
      return *msg;
   }

   return UnknownMessage{.raw = std::string(line)};
}

std::string debug_summary(const SetDifficultyMessage& msg) {
   std::ostringstream out;
   out << "set_difficulty:\n";
   out << "  difficulty: " << msg.difficulty << '\n';
   return out.str();
}

std::string debug_summary(const NotifyMessage& msg) {
   std::ostringstream out;
   out << "notify:\n";
   out << "  job_id:        " << msg.job_id << '\n';
   out << "  prevhash:      " << msg.prevhash << '\n';
   out << "  coinb1 bytes:  " << (msg.coinb1.size() / 2) << '\n';
   out << "  coinb2 bytes:  " << (msg.coinb2.size() / 2) << '\n';
   out << "  branches:      " << msg.merkle_branch.size() << '\n';
   out << "  version:       " << msg.version << '\n';
   out << "  nbits:         " << msg.nbits << '\n';
   out << "  ntime:         " << msg.ntime << '\n';
   out << "  clean_jobs:    " << (msg.clean_jobs ? "true" : "false") << '\n';

   if (!msg.merkle_branch.empty()) {
      out << "  first branch:  " << msg.merkle_branch.front() << '\n';
   }

   return out.str();
}

std::string debug_summary(const SubscribeResponse& msg) {
   std::ostringstream out;
   out << "subscribe:\n";
   out << "  id:               " << msg.id << '\n';
   out << "  extranonce1:      " << msg.extranonce1 << '\n';
   out << "  extranonce2_size: " << msg.extranonce2_size << " bytes\n";
   return out.str();
}

std::string debug_summary(const AuthorizeResponse& msg) {
   std::ostringstream out;
   out << "authorize:\n";
   out << "  id:       " << msg.id << '\n';
   out << "  accepted: " << (msg.accepted ? "true" : "false") << '\n';
   return out.str();
}

std::string debug_summary(const SubmitResponse& msg) {
   std::ostringstream out;
   out << "submit:\n";
   out << "  id:        " << msg.id << '\n';
   out << "  accepted:  " << (msg.accepted ? "true" : "false") << '\n';
   out << "  has_error: " << (msg.has_error ? "true" : "false") << '\n';
   if (msg.has_error) {
      out << "  error:     " << msg.error_text << '\n';
   }
   return out.str();
}

std::string debug_summary(const UnknownMessage& msg) {
   std::ostringstream out;
   out << "unknown:\n";
   out << "  raw: " << msg.raw << '\n';
   return out.str();
}

} // namespace cpu_miner

