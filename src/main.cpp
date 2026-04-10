// src/main.cpp

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include "mining_job/coinbase.hpp"
#include "mining_job/coordinator.hpp"
#include "mining_job/cpu_backend.hpp"
#include "mining_job/header.hpp"
#include "mining_job/scan.hpp"
#include "mining_job/target.hpp"
#include "mining_job/work_state.hpp"
#include "sha256/sha256.hpp"
#include "stratum_client/stratum_client.hpp"
#include "util/hex.hpp"
#include "util/uint256.hpp"

namespace {

volatile std::sig_atomic_t g_sigint_requested = 0;

void handle_sigint(int) { g_sigint_requested = 1; }

void print_startup_sanity(const cpu_miner::WorkState& work) {
   std::cout << "coinbase demo:\n";
   std::cout << "  extranonce2: " << work.coinbase.extranonce2_hex << '\n';
   std::cout << "  coinbase hex chars: " << work.coinbase.coinbase_hex.size()
             << '\n';
   std::cout << "  coinbase bytes: " << work.coinbase.coinbase_bytes.size()
             << '\n';

   const std::size_t preview_len =
      std::min<std::size_t>(work.coinbase.coinbase_hex.size(), 64U);

   std::cout << "  coinbase prefix: "
             << work.coinbase.coinbase_hex.substr(0, preview_len) << '\n';
   std::cout << "  coinbase suffix: "
             << work.coinbase.coinbase_hex.substr(
                   work.coinbase.coinbase_hex.size() > 192
                      ? work.coinbase.coinbase_hex.size() - 192
                      : 0)
             << '\n';

   std::cout << "  coinbase_hash: "
             << cpu_miner::bytes_to_hex_fixed_msb(work.coinbase.coinbase_hash)
             << '\n';

   const auto decoded_coinbase =
      cpu_miner::decode_coinbase(work.coinbase.coinbase_bytes);
   cpu_miner::print_coinbase_decoded(decoded_coinbase, std::cout);

   std::cout << "  merkle_root: " << work.merkle_root_raw_hex << '\n';

   const std::uint32_t version = cpu_miner::u32_from_hex_be(work.job.version);
   const std::uint32_t ntime = cpu_miner::u32_from_hex_be(work.job.ntime);
   const std::uint32_t nbits = cpu_miner::u32_from_hex_be(work.job.nbits);
   const std::uint32_t nonce0 = 0U;

   const auto header =
      cpu_miner::make_sha_input_header_bytes(version, work.prevhash_sha_input,
                                             work.merkle_root_sha_input, ntime,
                                             nbits, nonce0);

   const std::string header_hex = cpu_miner::header_sha_input_hex(header);
   const auto header_hash_words = cpu_miner::sha256::dbl_sha256_words(header);
   const auto header_hash_bytes =
      cpu_miner::sha256::digest_words_to_bytes_be(header_hash_words);

   std::cout << "  header_hex: " << header_hex << '\n';
   std::cout << "  header_hash_nonce0: "
             << cpu_miner::bytes_to_hex_fixed_msb(header_hash_bytes) << '\n';

   auto template_copy = work.header_template;
   cpu_miner::set_header_nonce(template_copy, nonce0);

   const auto template_hash_words =
      cpu_miner::hash_header_template(template_copy);
   const auto template_hash_bytes =
      cpu_miner::sha256::digest_words_to_bytes_be(template_hash_words);

   std::cout << "  template_hash_nonce0: "
             << cpu_miner::bytes_to_hex_fixed_msb(template_hash_bytes) << '\n';
}

struct PublishedWork {
   cpu_miner::WorkState work;
   cpu_miner::u256::uint256 network_target{};
   cpu_miner::u256::uint256 share_target{};
   std::uint64_t generation{};
   double share_difficulty{};
};

struct SharedWorkState {
   std::mutex mutex;
   std::condition_variable cv;
   std::optional<PublishedWork> published;
};

struct QueuedShare {
   cpu_miner::ShareSubmission submission;
   cpu_miner::ShareCandidate candidate;
};

class ShareQueue {
 public:
   void push(QueuedShare item) {
      {
         std::lock_guard<std::mutex> lock(mutex_);
         queue_.push(std::move(item));
      }
      cv_.notify_one();
   }

   [[nodiscard]] bool try_pop(QueuedShare& out) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) return false;

      out = std::move(queue_.front());
      queue_.pop();
      return true;
   }

   [[nodiscard]] bool wait_pop_for(QueuedShare& out, std::stop_token stop_token,
                                   std::chrono::milliseconds timeout) {
      std::unique_lock<std::mutex> lock(mutex_);

      const auto pred = [this, &stop_token]() {
         return !queue_.empty() || stop_token.stop_requested();
      };

      if (!cv_.wait_for(lock, timeout, pred)) {
         return false;
      }

      if (stop_token.stop_requested() || queue_.empty()) {
         return false;
      }

      out = std::move(queue_.front());
      queue_.pop();
      return true;
   }

 private:
   std::mutex mutex_;
   std::condition_variable cv_;
   std::queue<QueuedShare> queue_;
};

struct Counters {
   std::atomic<std::uint64_t> hashes_done{0};
   std::atomic<std::uint64_t> shares_found{0};
   std::atomic<std::uint64_t> blocks_found{0};
   std::atomic<std::uint64_t> shares_accepted{0};
   std::atomic<std::uint64_t> shares_rejected{0};
   std::atomic<std::uint64_t> current_scan_hashes_done{0};
};

struct TotalsSnapshot {
   std::uint64_t hashes_done{};
   std::uint64_t shares_found{};
   std::uint64_t blocks_found{};
   std::uint64_t shares_accepted{};
   std::uint64_t shares_rejected{};
   std::uint64_t current_scan_hashes_done{};
};

TotalsSnapshot snapshot_counters(const Counters& counters) {
   return TotalsSnapshot{
      .hashes_done = counters.hashes_done.load(std::memory_order_relaxed),
      .shares_found = counters.shares_found.load(std::memory_order_relaxed),
      .blocks_found = counters.blocks_found.load(std::memory_order_relaxed),
      .shares_accepted =
         counters.shares_accepted.load(std::memory_order_relaxed),
      .shares_rejected =
         counters.shares_rejected.load(std::memory_order_relaxed),
      .current_scan_hashes_done =
         counters.current_scan_hashes_done.load(std::memory_order_relaxed),
   };
}

void print_running_totals(const TotalsSnapshot& totals) {
   std::cout << "running totals:\n";
   std::cout << "  hashes: " << totals.hashes_done << '\n';
   std::cout << "  shares found: " << totals.shares_found << '\n';
   std::cout << "  blocks found: " << totals.blocks_found << '\n';
   std::cout << "  shares accepted: " << totals.shares_accepted << '\n';
   std::cout << "  shares rejected: " << totals.shares_rejected << '\n';
}

void clear_status_line(bool& status_line_active) {
   if (!status_line_active) {
      return;
   }

   std::cout << '\r'
             << "                                                              "
                "                  "
             << '\r' << std::flush;
   status_line_active = false;
}

void print_status_line(const TotalsSnapshot& totals, bool& status_line_active) {
   const std::uint64_t live_hashes =
      totals.hashes_done + totals.current_scan_hashes_done;

   std::cout << '\r' << "hashes=" << live_hashes
             << " shares=" << totals.shares_found
             << " blocks=" << totals.blocks_found
             << " accepted=" << totals.shares_accepted
             << " rejected=" << totals.shares_rejected << std::flush;

   status_line_active = true;
}

struct StartupEvent {
   cpu_miner::WorkState work;
   double difficulty{};
   std::uint64_t share_difficulty{};
   std::string extranonce1;
   std::size_t extranonce2_size{};
};

struct WorkUpdateEvent {
   std::string job_id;
   std::string ntime_hex;
   bool clean_jobs{};
   double difficulty{};
   std::uint64_t generation{};
   std::string raw_notify;
   std::string parsed_summary;
};

struct ChunkStartedEvent {
   std::string job_id;
   std::string extranonce2_hex;
   std::uint64_t generation{};
   std::uint64_t nonce_begin{};
   std::uint64_t nonce_end{};
};

struct ShareFoundEvent {
   std::string job_id;
   std::string extranonce2_hex;
   std::uint64_t generation{};
   std::uint32_t nonce{};
   cpu_miner::sha256::DigestBytes hash{};
   cpu_miner::u256::uint256 share_target{};
   cpu_miner::u256::uint256 network_target{};
   bool block_candidate{};
   std::string coinbase_hex;
   std::string coinbase_hash_hex;
   std::string merkle_root_raw_hex;
};

struct ShareSubmitEvent {
   std::string job_id;
   std::string extranonce2_hex;
   std::string ntime_hex;
   std::string nonce_hex;
   std::uint64_t generation{};
   std::uint32_t nonce{};
   bool accepted{};
   std::string error_text;
   std::string raw_request;
   std::string raw_response;
};

struct StaleShareDiscardedEvent {
   std::string job_id;
   std::uint32_t nonce{};
   std::uint64_t candidate_generation{};
   std::uint64_t current_generation{};
};

struct ScanFinishedEvent {
   std::string job_id;
   std::string extranonce2_hex;
   std::uint64_t generation{};
   cpu_miner::ScanResult result;
};

struct ShutdownEvent {
   std::string reason;
};

struct ErrorEvent {
   std::string source;
   std::string message;
};

struct ThreadExitedEvent {
   std::string thread_name;
};

using AppEvent = std::variant<StartupEvent, WorkUpdateEvent, ChunkStartedEvent,
                              ShareFoundEvent, ShareSubmitEvent,
                              StaleShareDiscardedEvent, ScanFinishedEvent,
                              ShutdownEvent, ErrorEvent, ThreadExitedEvent>;

class EventQueue {
 public:
   void push(AppEvent event) {
      {
         std::lock_guard<std::mutex> lock(mutex_);
         queue_.push(std::move(event));
      }
      cv_.notify_one();
   }

   [[nodiscard]] bool try_pop(AppEvent& out) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (queue_.empty()) return false;

      out = std::move(queue_.front());
      queue_.pop();
      return true;
   }

   [[nodiscard]] bool wait_pop_for(AppEvent& out,
                                   std::chrono::milliseconds timeout) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (!cv_.wait_for(lock, timeout, [this]() { return !queue_.empty(); })) {
         return false;
      }

      out = std::move(queue_.front());
      queue_.pop();
      return true;
   }

 private:
   std::mutex mutex_;
   std::condition_variable cv_;
   std::queue<AppEvent> queue_;
};

bool drain_share_queue(cpu_miner::StratumClient& client,
                       ShareQueue& share_queue, EventQueue& events,
                       Counters& counters,
                       const std::atomic<std::uint64_t>& work_generation) {
   bool did_work = false;

   QueuedShare queued;
   while (share_queue.try_pop(queued)) {
      did_work = true;

      const std::uint64_t current_generation =
         work_generation.load(std::memory_order_acquire);

      const auto& candidate = queued.candidate;

      if (candidate.generation != current_generation) {
         events.push(StaleShareDiscardedEvent{
            .job_id = candidate.work.job.job_id,
            .nonce = candidate.nonce,
            .candidate_generation = candidate.generation,
            .current_generation = current_generation,
         });
         continue;
      }

      const auto& submission = queued.submission;
      const auto submit_result = client.submit_share(submission);

      if (submit_result.accepted) {
         counters.shares_accepted.fetch_add(1U, std::memory_order_relaxed);
      } else {
         counters.shares_rejected.fetch_add(1U, std::memory_order_relaxed);
      }

      events.push(ShareSubmitEvent{
         .job_id = candidate.work.job.job_id,
         .extranonce2_hex = candidate.work.coinbase.extranonce2_hex,
         .ntime_hex = candidate.work.job.ntime,
         .nonce_hex = submission.nonce_hex,
         .generation = candidate.generation,
         .nonce = candidate.nonce,
         .accepted = submit_result.accepted,
         .error_text = submit_result.error_text,
         .raw_request = submit_result.raw_request,
         .raw_response = submit_result.raw_response,
      });
   }

   return did_work;
}

std::optional<PublishedWork>
wait_for_published_work(SharedWorkState& shared_work,
                        std::stop_token stop_token) {
   for (;;) {
      if (stop_token.stop_requested()) {
         return std::nullopt;
      }

      std::unique_lock<std::mutex> lock(shared_work.mutex);
      if (shared_work.published.has_value()) {
         return *shared_work.published;
      }

      shared_work.cv.wait_for(lock, std::chrono::milliseconds(50));
   }
}

void handle_found_share(const cpu_miner::ShareSubmission& submission,
                        const cpu_miner::ShareCandidate& candidate,
                        const PublishedWork& published, ShareQueue& share_queue,
                        EventQueue& events, Counters& counters) {
   counters.shares_found.fetch_add(1U, std::memory_order_relaxed);
   if (candidate.is_block_candidate) {
      counters.blocks_found.fetch_add(1U, std::memory_order_relaxed);
   }

   share_queue.push(QueuedShare{
      .submission = submission,
      .candidate = candidate,
   });

   events.push(ShareFoundEvent{
      .job_id = candidate.work.job.job_id,
      .extranonce2_hex = candidate.work.coinbase.extranonce2_hex,
      .generation = candidate.generation,
      .nonce = candidate.nonce,
      .hash = candidate.hash,
      .share_target = published.share_target,
      .network_target = published.network_target,
      .block_candidate = candidate.is_block_candidate,
      .coinbase_hex = candidate.work.coinbase.coinbase_hex,
      .coinbase_hash_hex = cpu_miner::bytes_to_hex_fixed_msb(
         candidate.work.coinbase.coinbase_hash),
      .merkle_root_raw_hex = candidate.work.merkle_root_raw_hex,
   });
}

void publish_latest_work(SharedWorkState& shared_work,
                         std::atomic<std::uint64_t>& work_generation,
                         const cpu_miner::StratumClient& client,
                         EventQueue& events) {
   if (!(client.subscription() && client.current_job())) {
      return;
   }

   const auto& sub = *client.subscription();
   const auto& job = *client.current_job();

   PublishedWork next;
   next.work = cpu_miner::make_work_state(job, sub, 0U);

   const std::uint32_t nbits = cpu_miner::u32_from_hex_be(job.nbits);
   next.network_target = cpu_miner::expand_compact_target(nbits);

   next.share_difficulty = client.difficulty();
   next.share_target =
      cpu_miner::share_target_from_difficulty(next.share_difficulty);

   next.generation =
      work_generation.fetch_add(1U, std::memory_order_acq_rel) + 1U;

   {
      std::lock_guard<std::mutex> lock(shared_work.mutex);
      shared_work.published = next;
   }

   shared_work.cv.notify_all();

   events.push(WorkUpdateEvent{
      .job_id = job.job_id,
      .ntime_hex = job.ntime,
      .clean_jobs = job.clean_jobs,
      .difficulty = client.difficulty(),
      .generation = next.generation,
      .raw_notify = client.last_raw_notify(),
      .parsed_summary = client.last_parsed_summary(),
   });
}

void record_thread_exception(std::mutex& error_mutex,
                             std::exception_ptr& first_error,
                             EventQueue& events, std::string source) {
   try {
      throw;
   } catch (const std::exception& ex) {
      {
         std::lock_guard<std::mutex> lock(error_mutex);
         if (!first_error) {
            first_error = std::current_exception();
         }
      }
      events.push(
         ErrorEvent{.source = std::move(source), .message = ex.what()});
   } catch (...) {
      {
         std::lock_guard<std::mutex> lock(error_mutex);
         if (!first_error) {
            first_error = std::current_exception();
         }
      }
      events.push(ErrorEvent{.source = std::move(source),
                             .message = "unknown exception"});
   }
}

void print_scan_finished(const ScanFinishedEvent& event) {
   std::cout << "scan result:\n";
   std::cout << "  job_id: " << event.job_id << '\n';
   std::cout << "  extranonce2: " << event.extranonce2_hex << '\n';
   std::cout << "  generation: " << event.generation << '\n';
   std::cout << "  scanned hashes: " << event.result.hashes_done << '\n';
   std::cout << "  shares found: " << event.result.shares_found << '\n';
   std::cout << "  blocks found: " << event.result.blocks_found << '\n';
   std::cout << "  elapsed seconds: " << std::fixed << std::setprecision(3)
             << event.result.elapsed_seconds << '\n';
   std::cout << "  average rate: " << std::setprecision(0)
             << event.result.hash_rate_hps << " H/s\n";
   std::cout << "  stop reason: ";
   switch (event.result.stop_reason) {
   case cpu_miner::ScanStopReason::exhausted:
      std::cout << "exhausted\n";
      break;
   case cpu_miner::ScanStopReason::stale:
      std::cout << "stale\n";
      break;
   case cpu_miner::ScanStopReason::stop_requested:
      std::cout << "stop_requested\n";
      break;
   }
}

struct ScanChunk {
   std::uint64_t nonce_begin{};
   std::uint64_t nonce_end{};
   std::uint64_t hashes{};
};

constexpr std::uint64_t kNonceChunkSize =
   static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL;
constexpr std::uint64_t kProgressInterval = 1'000'000ULL;
constexpr std::uint64_t kMaxNonce =
   static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max());

ScanChunk make_scan_chunk(std::uint32_t nonce_begin) {
   const std::uint64_t begin = nonce_begin;
   const std::uint64_t remaining = (kMaxNonce - begin) + 1ULL;
   const std::uint64_t hashes = std::min(kNonceChunkSize, remaining);
   const std::uint64_t end = begin + hashes - 1ULL;

   return ScanChunk{
      .nonce_begin = begin,
      .nonce_end = end,
      .hashes = hashes,
   };
}

cpu_miner::ScanControl make_scan_control(
   std::stop_token stop_token,
   std::atomic<std::uint64_t>& work_generation,
   std::uint64_t expected_generation,
   std::atomic<std::uint64_t>& current_scan_hashes_done) {
   return cpu_miner::ScanControl{
      .stop_token = stop_token,
      .work_generation = &work_generation,
      .expected_generation = expected_generation,
      .check_interval = 16'384U,
      .progress_hashes_done = &current_scan_hashes_done,
   };
}

cpu_miner::ScanResult run_scan_chunk(
   cpu_miner::MiningCoordinator& coordinator, const PublishedWork& published,
   cpu_miner::WorkState& work, std::uint64_t nonce_begin,
   std::uint64_t nonce_end, std::stop_token stop_token,
   std::atomic<std::uint64_t>& work_generation, ShareQueue& share_queue,
   EventQueue& events, Counters& counters) {
   events.push(ChunkStartedEvent{
      .job_id = work.job.job_id,
      .extranonce2_hex = work.coinbase.extranonce2_hex,
      .generation = published.generation,
      .nonce_begin = nonce_begin,
      .nonce_end = nonce_end,
   });

   const auto control =
      make_scan_control(stop_token, work_generation, published.generation,
                        counters.current_scan_hashes_done);

   coordinator.on_share_found([&](const cpu_miner::ShareSubmission& submission,
                                  const cpu_miner::ShareCandidate& candidate) {
      handle_found_share(submission, candidate, published, share_queue, events,
                         counters);
   });

   const auto result =
      coordinator.scan_range(nonce_begin, nonce_end, published.network_target,
                             published.share_target, kProgressInterval, control);

   counters.hashes_done.fetch_add(result.hashes_done, std::memory_order_relaxed);
   counters.current_scan_hashes_done.store(0U, std::memory_order_relaxed);

   events.push(ScanFinishedEvent{
      .job_id = work.job.job_id,
      .extranonce2_hex = work.coinbase.extranonce2_hex,
      .generation = published.generation,
      .result = result,
   });

   return result;
}

enum class WorkerNextAction {
   adopt_new_work,
   exit_thread,
   continue_scanning,
};

WorkerNextAction handle_scan_result(const cpu_miner::ScanResult& result,
                                    cpu_miner::WorkState& work,
                                    cpu_miner::MiningCoordinator& coordinator,
                                    std::uint64_t nonce_end,
                                    EventQueue& events) {
   if (result.stop_reason == cpu_miner::ScanStopReason::stale) {
      return WorkerNextAction::adopt_new_work;
   }

   if (result.stop_reason == cpu_miner::ScanStopReason::stop_requested) {
      events.push(ThreadExitedEvent{.thread_name = "worker"});
      return WorkerNextAction::exit_thread;
   }

   if (nonce_end == kMaxNonce) {
      cpu_miner::advance_extranonce2(work);
      coordinator.set_job(work.job, work.subscription, work.extranonce2_counter);
      return WorkerNextAction::continue_scanning;
   }

   work.nonce = static_cast<std::uint32_t>(nonce_end + 1ULL);
   return WorkerNextAction::continue_scanning;
}

} // namespace

int main(int argc, char* argv[]) {
   try {
      std::signal(SIGINT, handle_sigint);

      const std::string host = (argc > 1) ? argv[1] : "192.168.0.104";
      const std::string port = (argc > 2) ? argv[2] : "3333";
      const std::string user =
         (argc > 3) ? argv[3] : "bc1qyourwalletaddresshere.cpu-miner";
      const std::string password = (argc > 4) ? argv[4] : "x";

      SharedWorkState shared_work;
      ShareQueue share_queue;
      EventQueue events;
      Counters counters;
      std::atomic<std::uint64_t> work_generation{0};

      std::mutex error_mutex;
      std::exception_ptr first_error;

      std::atomic<bool> startup_announced{false};

      std::jthread control_thread([&](std::stop_token stop_token) {
         try {
            cpu_miner::StratumClient client(host, port);

            client.connect();
            client.subscribe();
            client.suggest_difficulty(1.0);
            client.authorize(user, password);
            client.run_until_ready();

            if (!(client.subscription() && client.current_job())) {
               throw std::runtime_error(
                  "missing subscription or current job after startup");
            }

            publish_latest_work(shared_work, work_generation, client, events);

            if (!startup_announced.exchange(true, std::memory_order_acq_rel)) {
               const auto& published = *shared_work.published;
               events.push(StartupEvent{
                  .work = published.work,
                  .difficulty = client.difficulty(),
                  .share_difficulty =
                     static_cast<uint64_t>(published.share_difficulty),
                  .extranonce1 = published.work.subscription.extranonce1,
                  .extranonce2_size =
                     published.work.subscription.extranonce2_size,
               });
            }

            while (!stop_token.stop_requested()) {
               bool did_work = drain_share_queue(client, share_queue, events,
                                                 counters, work_generation);

               const auto poll = client.poll();
               if (poll.work_invalidated) {
                  publish_latest_work(shared_work, work_generation, client,
                                      events);
               }

               if (poll.got_message) {
                  did_work = true;
               }

               if (did_work) {
                  continue;
               }

               QueuedShare queued;
               (void)share_queue.wait_pop_for(queued, stop_token,
                                              std::chrono::milliseconds(20));
               if (!stop_token.stop_requested()) {
                  if (!queued.candidate.work.empty()) {
                     share_queue.push(std::move(queued));
                  }
               }
            }
         } catch (...) {
            record_thread_exception(error_mutex, first_error, events,
                                    "control");
         }

         events.push(ThreadExitedEvent{.thread_name = "control"});
      });

      std::jthread worker_thread([&](std::stop_token stop_token) {
         try {
            cpu_miner::CpuHasherBackend backend;
            cpu_miner::MiningCoordinator coordinator{backend};

            for (;;) {
               const auto maybe_published =
                  wait_for_published_work(shared_work, stop_token);
               if (!maybe_published.has_value()) {
                  events.push(ThreadExitedEvent{.thread_name = "worker"});
                  return;
               }

               auto published = *maybe_published;
               auto work = published.work;

               coordinator.set_job(work.job, work.subscription,
                                   work.extranonce2_counter);

               while (!stop_token.stop_requested()) {
                  const ScanChunk chunk = make_scan_chunk(work.nonce);
                  const std::uint64_t nonce_begin = chunk.nonce_begin;
                  const std::uint64_t nonce_end = chunk.nonce_end;

                  const auto result =
                     run_scan_chunk(coordinator, published, work, nonce_begin,
                                    nonce_end, stop_token, work_generation,
                                    share_queue, events, counters);

                  switch (handle_scan_result(result, work, coordinator,
                                             nonce_end, events)) {
                  case WorkerNextAction::adopt_new_work:
                     break;
                  case WorkerNextAction::exit_thread:
                     return;
                  case WorkerNextAction::continue_scanning:
                     continue;
                  }
                  break;
               }
            }
         } catch (...) {
            record_thread_exception(error_mutex, first_error, events, "worker");
         }

         events.push(ThreadExitedEvent{.thread_name = "worker"});
      });

      bool stop_requested = false;
      bool control_exited = false;
      bool worker_exited = false;
      auto last_status_print = std::chrono::steady_clock::now();
      bool status_line_active = false;

      while (!(control_exited && worker_exited)) {
         if (g_sigint_requested != 0 && !stop_requested) {
            stop_requested = true;
            events.push(ShutdownEvent{
               .reason = "SIGINT received; requesting graceful shutdown"});
            control_thread.request_stop();
            worker_thread.request_stop();
         }

         {
            std::lock_guard<std::mutex> lock(error_mutex);
            if (first_error && !stop_requested) {
               stop_requested = true;
               events.push(ShutdownEvent{
                  .reason = "fatal worker/control error; requesting shutdown"});
               control_thread.request_stop();
               worker_thread.request_stop();
            }
         }

         AppEvent event;
         if (events.wait_pop_for(event, std::chrono::milliseconds(100))) {
            std::visit(
               [&](const auto& e) {
                  using T = std::decay_t<decltype(e)>;

                  clear_status_line(status_line_active);

                  if constexpr (std::is_same_v<T, StartupEvent>) {
                     std::cout << "Difficulty: " << e.difficulty << "\n";
                     print_startup_sanity(e.work);
                     std::cout << "subscription:\n";
                     std::cout << "  extranonce1: " << e.extranonce1 << '\n';
                     std::cout << "  extranonce2_size: " << e.extranonce2_size
                               << '\n';

                     const std::uint32_t nbits =
                        cpu_miner::u32_from_hex_be(e.work.job.nbits);

                     std::cout << "mining setup:\n";
                     std::cout << "  nbits: 0x" << std::hex << std::setw(8)
                               << std::setfill('0') << nbits << std::dec
                               << std::setfill(' ') << '\n';
                     std::cout << "  share difficulty: " << e.share_difficulty
                               << '\n';
                  } else if constexpr (std::is_same_v<T, WorkUpdateEvent>) {
                     std::cout << "work update:\n";
                     std::cout << "  generation: " << e.generation << '\n';
                     std::cout << "  job_id: " << e.job_id << '\n';
                     std::cout << "  ntime: " << e.ntime_hex << '\n';
                     std::cout
                        << "  clean_jobs: " << (e.clean_jobs ? "true" : "false")
                        << '\n';
                     std::cout << "  difficulty: " << e.difficulty << '\n';
                     if (!e.raw_notify.empty()) {
                        std::cout << "  raw notify: " << e.raw_notify << '\n';
                     }
                     if (!e.parsed_summary.empty()) {
                        std::cout << "  parsed notify summary:\n";
                        std::cout << e.parsed_summary;
                     }
                  } else if constexpr (std::is_same_v<T, ChunkStartedEvent>) {
                     std::cout << "mining chunk:\n";
                     std::cout << "  generation: " << e.generation << '\n';
                     std::cout << "  job_id: " << e.job_id << '\n';
                     std::cout << "  extranonce2: " << e.extranonce2_hex
                               << '\n';
                     std::cout << "  nonce range: [" << e.nonce_begin << ", "
                               << e.nonce_end << "]\n";
                  } else if constexpr (std::is_same_v<T, ShareFoundEvent>) {
                     std::cout
                        << "  "
                        << (e.block_candidate ? "BLOCK CANDIDATE" : "SHARE HIT")
                        << " generation=" << e.generation
                        << " nonce=" << e.nonce << '\n';

                     // --- hash ---
                     std::cout << "    hash (display, BE):  "
                               << cpu_miner::bytes_to_hex_fixed_msb(e.hash)
                               << '\n';
                     std::cout << "    hash (raw bytes):    "
                               << cpu_miner::bytes_to_hex(e.hash) << '\n';

                     // --- targets ---
                     std::cout << "    share target:        "
                               << e.share_target.to_hex_be_fixed() << '\n';
                     std::cout << "    network target:      "
                               << e.network_target.to_hex_be_fixed() << '\n';

                     // --- comparison (authoritative path) ---
                     const auto meets_share =
                        cpu_miner::hash_meets_target(e.hash, e.share_target);
                     const auto meets_network =
                        cpu_miner::hash_meets_target(e.hash, e.network_target);

                     std::cout << "    meets share target:  "
                               << (meets_share ? "true" : "false") << '\n';
                     std::cout << "    meets network target:"
                               << (meets_network ? " true" : " false") << '\n';

                     // --- construction diagnostics ---
                     std::cout << "    coinbase hex:        " << e.coinbase_hex
                               << '\n';
                     std::cout
                        << "    coinbase hash (BE):  " << e.coinbase_hash_hex
                        << '\n';
                     std::cout
                        << "    merkle root (BE):    " << e.merkle_root_raw_hex
                        << '\n';
                  } else if constexpr (std::is_same_v<T, ShareSubmitEvent>) {
                     std::cout << "  share submission: "
                               << (e.accepted ? "accepted" : "rejected")
                               << '\n';
                     std::cout << "    generation: " << e.generation << '\n';
                     std::cout << "    job_id: " << e.job_id << '\n';
                     std::cout << "    extranonce2: " << e.extranonce2_hex
                               << '\n';
                     std::cout << "    ntime: " << e.ntime_hex << '\n';
                     std::cout << "    nonce: " << e.nonce << '\n';
                     std::cout << "    nonce_hex: " << e.nonce_hex << '\n';
                     if (!e.error_text.empty()) {
                        std::cout << "    error: " << e.error_text << '\n';
                     }
                     if (!e.raw_request.empty()) {
                        std::cout << "    raw request:  " << e.raw_request
                                  << '\n';
                     }
                     if (!e.raw_response.empty()) {
                        std::cout << "    raw response: " << e.raw_response
                                  << '\n';
                     }
                     print_running_totals(snapshot_counters(counters));
                  } else if constexpr (std::is_same_v<
                                          T, StaleShareDiscardedEvent>) {
                     std::cout << "stale share candidate discarded:\n";
                     std::cout << "  job_id: " << e.job_id << '\n';
                     std::cout << "  nonce: " << e.nonce << '\n';
                     std::cout
                        << "  candidate_generation: " << e.candidate_generation
                        << '\n';
                     std::cout
                        << "  current_generation: " << e.current_generation
                        << '\n';
                  } else if constexpr (std::is_same_v<T, ScanFinishedEvent>) {
                     print_scan_finished(e);
                     print_running_totals(snapshot_counters(counters));
                  } else if constexpr (std::is_same_v<T, ShutdownEvent>) {
                     std::cout << e.reason << '\n';
                  } else if constexpr (std::is_same_v<T, ErrorEvent>) {
                     std::cerr << "error[" << e.source << "]: " << e.message
                               << '\n';
                  } else if constexpr (std::is_same_v<T, ThreadExitedEvent>) {
                     std::cout << "thread exited: " << e.thread_name << '\n';
                     if (e.thread_name == "control") {
                        control_exited = true;
                     } else if (e.thread_name == "worker") {
                        worker_exited = true;
                     }
                  }
               },
               event);
         }

         const auto now = std::chrono::steady_clock::now();
         if (now - last_status_print >= std::chrono::seconds(2)) {
            last_status_print = now;
            if (!stop_requested) {
               print_status_line(snapshot_counters(counters),
                                 status_line_active);
            }
         }
      }

      AppEvent leftover;
      while (events.try_pop(leftover)) {
         std::visit(
            [&](const auto& e) {
               using T = std::decay_t<decltype(e)>;
               if constexpr (std::is_same_v<T, ErrorEvent>) {
                  std::cerr << "error[" << e.source << "]: " << e.message
                            << '\n';
               }
            },
            leftover);
      }

      clear_status_line(status_line_active);

      std::cout << "final totals:\n";
      print_running_totals(snapshot_counters(counters));

      {
         std::lock_guard<std::mutex> lock(error_mutex);
         if (first_error) {
            std::rethrow_exception(first_error);
         }
      }

      return 0;
   } catch (const std::exception& ex) {
      std::cerr << "fatal: " << ex.what() << '\n';
      return 1;
   }
}

