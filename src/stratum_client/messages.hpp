// src/stratum_client/messages.hpp

#ifndef CPU_MINER_STRATUM_CLIENT_MESSAGES_HPP
#define CPU_MINER_STRATUM_CLIENT_MESSAGES_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cpu_miner {

enum class MessageType {
   unknown,
   set_difficulty,
   notify,
   subscribe_response,
   authorize_response,
   submit_response,
};

struct SetDifficultyMessage {
   static constexpr MessageType type = MessageType::set_difficulty;
   double difficulty{};
};

struct NotifyMessage {
   static constexpr MessageType type = MessageType::notify;

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
   static constexpr MessageType type = MessageType::subscribe_response;

   int id{};
   std::string extranonce1;
   std::size_t extranonce2_size{};
};

struct AuthorizeResponse {
   static constexpr MessageType type = MessageType::authorize_response;

   int id{};
   bool accepted{};
};

struct SubmitResponse {
   static constexpr MessageType type = MessageType::submit_response;

   int id{};
   bool accepted{};
   bool has_error{};
   std::string error_text;
};

struct UnknownMessage {
   static constexpr MessageType type = MessageType::unknown;

   std::string raw;
};

using IncomingMessage =
   std::variant<UnknownMessage, SetDifficultyMessage, NotifyMessage,
                SubscribeResponse, AuthorizeResponse, SubmitResponse>;

struct SubscribeRequest {};

struct SuggestDifficultyRequest {
   double difficulty{};
};

struct AuthorizeRequest {
   std::string user;
   std::string password;
};

struct SubmitShareRequest {
   std::string worker_name;
   std::string job_id;
   std::string extranonce2_hex;
   std::string ntime_hex;
   std::string nonce_hex;
};

[[nodiscard]] std::string to_wire_message(const SubscribeRequest& request,
                                          int id);

[[nodiscard]] std::string
to_wire_message(const SuggestDifficultyRequest& request, int id);

[[nodiscard]] std::string to_wire_message(const AuthorizeRequest& request,
                                          int id);

[[nodiscard]] std::string to_wire_message(const SubmitShareRequest& request,
                                          int id);

[[nodiscard]] std::optional<IncomingMessage>
parse_incoming_message(std::string_view line);

[[nodiscard]] std::string debug_summary(const SetDifficultyMessage& msg);
[[nodiscard]] std::string debug_summary(const NotifyMessage& msg);
[[nodiscard]] std::string debug_summary(const SubscribeResponse& msg);
[[nodiscard]] std::string debug_summary(const AuthorizeResponse& msg);
[[nodiscard]] std::string debug_summary(const SubmitResponse& msg);
[[nodiscard]] std::string debug_summary(const UnknownMessage& msg);

} // namespace cpu_miner

#endif

