#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// SHA-256 hashing using OpenSSL EVP API
std::string double_sha256(const std::string &input) {
    // Temporary buffer for intermediate and final hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    // First SHA-256 hash
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create EVP_MD_CTX");

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize first SHA-256 context");
    }
    if (EVP_DigestUpdate(mdctx, input.data(), input.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update first SHA-256 hash");
    }
    if (EVP_DigestFinal_ex(mdctx, hash, &length) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize first SHA-256 hash");
    }

    // Reinitialize the context for the second SHA-256 hash
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize second SHA-256 context");
    }
    if (EVP_DigestUpdate(mdctx, hash, length) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update second SHA-256 hash");
    }
    if (EVP_DigestFinal_ex(mdctx, hash, &length) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize second SHA-256 hash");
    }

    EVP_MD_CTX_free(mdctx);

    // Convert the final hash to a hexadecimal string
    std::ostringstream ss;
    for (unsigned int i = 0; i < length; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

// Helper to print JSON responses (for debugging)
void print_json(const json::value &jv) {
    std::cout << json::serialize(jv) << std::endl;
}

class MiningJob {
public:
    // Placeholder for job details like target difficulty, block header, etc.
};

class Miner {
public:
    Miner() : running(false) {}

    // Starts mining with the current job.
    void start(const MiningJob& job) {
        stop(); // Stop any current mining work
        running = true;
        worker = std::thread([this, job]() { mine(job); });
    }

    // Stops the mining thread.
    void stop() {
        running = false;
        if (worker.joinable()) {
            worker.join();
        }
    }

    ~Miner() {
        stop();
    }

private:
    std::atomic<bool> running;
    std::thread worker;

    // Simulated mining function
    void mine(const MiningJob& job) {
        while (running) {
            // Mining logic goes here
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate work
            std::cout << "Mining...\n";
        }
    }
};

class EventLoop {
public:
    EventLoop()
    : io_context(), work_guard(boost::asio::make_work_guard(io_context)), miner() {}

    // Starts the event loop
    void run() {
        // Simulate receiving jobs from Stratum server asynchronously
        std::thread jobListener([this]() {
            for (int i = 0; i < 5; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Simulate delay
                receiveNewJob();
            }
            io_context.stop(); // Stop the loop after jobs are processed
        });

        io_context.run(); // Main thread runs the io_context loop
        jobListener.join();
    }

private:
    boost::asio::io_context io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
    Miner miner;

    // Receives a new job from the Stratum server and starts mining
    void receiveNewJob() {
        MiningJob newJob;
        std::cout << "Received new job.\n";
        miner.start(newJob); // Start or restart mining with the new job
    }
};

int main() {
    EventLoop eventLoop;
    eventLoop.run();
    return 0;
}

