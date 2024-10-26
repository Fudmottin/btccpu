#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <openssl/sha.h>
#include <openssl/evp.h>

// Aliases for convenience
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// SHA-256 hashing using the EVP API (OpenSSL 3.0+)
std::string sha256(const std::string &input) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create EVP_MD_CTX");

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize SHA-256 context");
    }

    if (EVP_DigestUpdate(mdctx, input.c_str(), input.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update SHA-256 hash");
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    if (EVP_DigestFinal_ex(mdctx, hash, &length) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize SHA-256 hash");
    }

    EVP_MD_CTX_free(mdctx);

    // Convert the hash to a hexadecimal string
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

// Miner class to handle Stratum connection and work
class Miner {
public:
    Miner(const std::string &host, const std::string &port,
          const std::string &user, const std::string &password)
        : io_context(), socket(io_context), host(host), port(port),
          user(user), password(password) {}

    void run() {
        connect();
        authenticate();
        listen_for_jobs();
    }

private:
    asio::io_context io_context;
    tcp::socket socket;
    std::string host, port, user, password;

    // Connect to the Stratum pool
    void connect() {
        asio::ip::address ip_address = asio::ip::make_address(host);
        tcp::endpoint endpoint(ip_address, std::stoi(port));
        socket.connect(endpoint);
        std::cout << "Connected to pool: " << host << ":" << port << std::endl;
    }

    // Authenticate with the Stratum server
    void authenticate() {
        json::object request;
        request["id"] = 1;
        request["method"] = "mining.subscribe";
        request["params"] = json::array{};

        send_request(request);

        request = {};
        request["id"] = 2;
        request["method"] = "mining.authorize";
        request["params"] = json::array{user, password};

        send_request(request);
    }

    // Send JSON request to the Stratum server
    void send_request(const json::object &request) {
        std::string request_str = json::serialize(request) + "\n";
        asio::write(socket, asio::buffer(request_str));
    }

    // Listen for new jobs and process them
    void listen_for_jobs() {
        try {
            for (;;) {  // Infinite loop to keep listening for jobs
                asio::streambuf response;
                asio::read_until(socket, response, "\n");

                std::istream is(&response);
                std::string line;
                std::getline(is, line);

                if (!line.empty()) {
                    auto job = json::parse(line).as_object();
                    print_json(job);  // Log the full job for debugging

                    if (job["method"] == "mining.notify") {
                        process_job(job["params"].as_array());
                    }
                } else {
                    std::cout << "No jobs received yet. Waiting..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "Error listening for jobs: " << e.what() << std::endl;
        }
    }

    // Process the received job from the pool
    void process_job(const json::array &params) {
        std::string job_id = json::value_to<std::string>(params[0]);
        std::string prev_hash = json::value_to<std::string>(params[1]);
        std::string coinbase_tx = json::value_to<std::string>(params[2]);

        std::cout << "Received job ID: " << job_id << std::endl;

        // Hash the coinbase transaction (this is simplified!)
        std::string coinbase_hash = sha256(coinbase_tx);
        std::cout << "Coinbase Hash: " << coinbase_hash << std::endl;

        // This would be the main loop, trying nonces to find a valid solution
        for (uint32_t nonce = 0; nonce < UINT32_MAX; ++nonce) {
            std::string header = prev_hash + coinbase_hash + std::to_string(nonce);
            std::string result = sha256(header);

            if (result.substr(0, 4) == "0000") {  // Simple difficulty check
                std::cout << "Valid share found! Nonce: " << nonce << std::endl;
                submit_share(job_id, nonce);
                break;
            }
        }
    }

    // Submit the found share back to the pool
    void submit_share(const std::string &job_id, uint32_t nonce) {
        json::object request;
        request["id"] = 3;
        request["method"] = "mining.submit";
        request["params"] = json::array{user, job_id, std::to_string(nonce)};

        send_request(request);
        std::cout << "Share submitted for job ID: " << job_id << std::endl;
    }
};

int main() {
    std::string host = "10.0.1.210";                                            // CKPool IP
    std::string port = "3333";                                                  // Standard stratum port
    std::string user = "bc1q7shp5h6pcgey0hsqy2k2rvrfegly8c9uaxrtfx.cpu_miner";  // Bitcoin wallet address for payout
    std::string password = "x";                                                 // Usually 'x' is used as a placeholder for Stratum


    try {
        Miner miner(host, port, user, password);
        miner.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

