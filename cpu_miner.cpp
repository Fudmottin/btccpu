#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <vector>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// SHA-256 hashing using OpenSSL EVP API
std::string sha256(const std::string &input) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create EVP_MD_CTX");

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize SHA-256 context");
    }

    if (EVP_DigestUpdate(mdctx, input.data(), input.size()) != 1) {
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

    std::string subscription_id, extranonce1, worker_name;
    uint32_t extranonce2_size = 0, extranonce2 = 0;
    double difficulty = 1.0;

    std::string current_job_id, prev_hash, coinbase_tx1, coinbase_tx2;
    std::vector<std::string> merkle_branch;
    std::string version, nbits, ntime;

    void connect() {
        try {
            tcp::resolver resolver(io_context);
            auto endpoints = resolver.resolve(host, port);
            asio::connect(socket, endpoints);
            std::cout << "Connected to pool: " << host << ":" << port << std::endl;
        } catch (const std::exception &e) {
            throw std::runtime_error("Connection failed: " + std::string(e.what()));
        }
    }

    void send_request(const json::object &request) {
        std::string request_str = json::serialize(request) + "\n";
        asio::write(socket, asio::buffer(request_str));
    }

    json::value receive_response() {
        asio::streambuf response;
        asio::read_until(socket, response, "\n");

        std::istream is(&response);
        std::string line;
        std::getline(is, line);

        return json::parse(line);
    }

    json::value send_and_receive(const json::object &request) {
        send_request(request);
        return receive_response();
    }

    void authenticate() {
        json::object subscribe_request = {
            {"id", 1},
            {"method", "mining.subscribe"},
            {"params", json::array{}}
        };
        auto subscribe_response = send_and_receive(subscribe_request);
        print_json(subscribe_response);

        const auto& result = subscribe_response.at("result").as_array();
        if (result.size() < 3) throw std::runtime_error("Invalid subscription response");

        subscription_id = result[0].as_array()[1].as_string().c_str();
        extranonce1 = result[1].as_string().c_str();
        extranonce2_size = result[2].as_int64();

        json::object auth_request = {
            {"id", 2},
            {"method", "mining.authorize"},
            {"params", json::array{user, password}}
        };
        auto auth_response = send_and_receive(auth_request);
        print_json(auth_response);

        if (!auth_response.at("result").as_bool()) {
            throw std::runtime_error("Authentication failed");
        }

        std::cout << "Successfully authenticated with the pool." << std::endl;
    }

    void process_job(const json::array &params) {
        if (params.size() < 8) throw std::runtime_error("Invalid job parameters");

        current_job_id = params[0].as_string().c_str();
        prev_hash = params[1].as_string().c_str();
        coinbase_tx1 = params[2].as_string().c_str();
        coinbase_tx2 = params[3].as_string().c_str();

        merkle_branch.clear();
        for (const auto &branch : params[4].as_array()) {
            merkle_branch.push_back(branch.as_string().c_str());
        }

        version = params[5].as_string().c_str();
        nbits = params[6].as_string().c_str();
        ntime = params[7].as_string().c_str();

        extranonce2 = 0;

        std::cout << "Received new job ID: " << current_job_id << std::endl;
    }

    void listen_for_jobs() {
        try {
            while (true) {
                auto job = receive_response().as_object();
                print_json(job);

                if (job.contains("method") && job["method"] == "mining.notify") {
                    process_job(job["params"].as_array());
                } else {
                    std::cout << "Waiting for jobs..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
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
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

