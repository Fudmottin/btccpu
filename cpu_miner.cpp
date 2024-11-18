#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <deque>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <functional>

namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

// Helper to print JSON with logging context
void print_json(const std::string& context, const json::value& jv) {
    std::cout << "[" << context << "] " << json::serialize(jv) << "\n";
}

class EventLoop;

class MiningJob {
public:
    std::string job_id;
    std::string prevhash;
    std::string coinb1;
    std::string coinb2;
    std::string merkle_branch;
};

class Miner {
public:
    Miner(EventLoop& eventLoopRef)
        : running(false), eventLoop(eventLoopRef) {}

    void start(const MiningJob& job) {
        stop();
        currentJob = job;
        running = true;
        worker = std::thread([this]() { mine(); });
    }

    void stop() {
        running = false;
        if (worker.joinable()) {
            worker.join();
        }
    }

    ~Miner() {
        stop();
    }

    void handleMessage(const json::value& message) {
        if (!message.is_object()) {
            std::cerr << "Invalid message format.\n";
            return;
        }
        const auto& obj = message.as_object();
        if (obj.contains("id") && obj.at("id").is_int64() && obj.at("id").as_int64() == 1) {
            std::cout << "Subscription successful.\n";
        } else if (obj.contains("method") && obj.at("method").as_string() == "mining.notify") {
            parseNewJob(obj.at("params"));
        }
    }

private:
    std::atomic<bool> running;
    std::thread worker;
    EventLoop& eventLoop;
    MiningJob currentJob;

    void mine() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "Mining...\n";
        }
    }

    void parseNewJob(const json::value& params) {
        if (!params.is_array()) {
            std::cerr << "Invalid job parameters.\n";
            return;
        }
        // Parse and start new mining job...
    }
};

class EventLoop {
public:
    EventLoop(Miner& minerRef)
    : io_context(),
      socket(io_context),
      resolver(io_context),
      miner(minerRef),
      work_guard(asio::make_work_guard(io_context)) { // Prevent io_context from stopping
          std::cout << "EventLoop: Initialized." << std::endl;
      }

    void connect(const std::string& host, const std::string& port) {
        std::cout << "Connecting to Stratum server at " << host << ":" << port << "..." << std::endl;
        auto endpoints = resolver.resolve(host, port);
        for (const auto& endpoint : endpoints) {
            std::cout << "Resolved endpoint: "
                      << endpoint.endpoint().address().to_string()
                      << ":"
                      << endpoint.endpoint().port()
                      << "\n";
        }
        asio::async_connect(socket, endpoints,
            [this](boost::system::error_code ec, tcp::endpoint endpoint) {
                if (!ec) {
                    std::cout << "Connected to: "
                              << endpoint.address().to_string() 
                              << ":" 
                              << endpoint.port() 
                              << "\n";
                    sendSubscribe();
                    startRead();
                } else {
                    std::cerr << "Connect failed: " << ec.message() << "\n";
                    io_context.stop();
                }
        });
    }

    void sendMessage(const json::value& message) {
        std::string serialized = json::serialize(message) + "\n";
        asio::post(io_context, [this, serialized]() {
            bool write_in_progress = !write_msgs.empty();
            write_msgs.push_back(serialized);
            if (!write_in_progress) {
                doWrite();
            }
        });
        print_json("Sent", message); // Log outgoing messages
    }

    void run() {
        std::cout << "EventLoop running..." << std::endl;
        std::cout << "EventLoop: Starting io_context.run()..." << std::endl;
        io_thread = std::thread([this]() {
            try {
                io_context.run();
                std::cout << "EventLoop: io_context.run() has finished." << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "EventLoop: Exception in run(): " << ex.what() << std::endl;
            }
        });
    }

    void stop() {
        asio::post(io_context, [this]() { socket.close(); });
        io_context.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }

    ~EventLoop() {
        stop();
    }

private:
    asio::io_context io_context;
    tcp::socket socket;
    tcp::resolver resolver;
    std::thread io_thread;
    Miner& miner;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::string read_buffer;
    std::deque<std::string> write_msgs;

    void sendSubscribe() {
        std::string subscribe_message = R"({"method":"mining.subscribe","params":[],"id":1}\n)";
        std::cout << "Sending: " << subscribe_message << std::endl;

        asio::async_write(socket, asio::buffer(subscribe_message),
            [this](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                std::cout << "EventLoop: Sent " << bytes_transferred << " bytes." << std::endl;
            } else {
                std::cerr << "EventLoop: Write failed: " << ec.message() << std::endl;
                io_context.stop();
            }
        });
    }

    void startRead() {
        std::cout << "EventLoop: Starting read operation..." << std::endl;
        asio::async_read_until(socket, asio::dynamic_buffer(read_buffer), '\n',
         [this](boost::system::error_code ec, std::size_t length) {
             if (!ec) {
                 std::cout << "EventLoop: Read " << length << " bytes." << std::endl;
                 std::string line(read_buffer.substr(0, length));
                 read_buffer.erase(0, length);
                 handleMessage(line);
                 startRead(); // Continue reading
             } else {
                 std::cerr << "EventLoop: Read failed: " << ec.message() << std::endl;
                 io_context.stop();
             }
         });
    }

    void handleMessage(const std::string& message) {
        try {
            json::value jv = json::parse(message);
            print_json("Received", jv); // Log incoming messages
            miner.handleMessage(jv);
        } catch (const std::exception& ex) {
            std::cerr << "Failed to parse JSON: " << ex.what() << "\n";
        }
    }

    void doWrite() {
        asio::async_write(socket, asio::buffer(write_msgs.front()),
            [this](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    write_msgs.pop_front();
                    if (!write_msgs.empty()) {
                        doWrite();
                    }
                } else {
                    std::cerr << "Write failed: " << ec.message() << "\n";
                    socket.close();
                }
            });
    }
};

int main() {
    try {
        std::cout << "Starting Miner setup..." << std::endl;

        Miner* miner = nullptr; // Forward declaration to break circular dependency
        EventLoop eventLoop(*miner); // Create EventLoop with an uninitialized reference

        std::cout << "EventLoop created." << std::endl;

        Miner minerObj(eventLoop); // Create Miner
        miner = &minerObj; // Assign the correct reference after initialization

        std::cout << "Miner created." << std::endl;

        std::string host = "10.0.1.210";
        std::string port = "3333";

        eventLoop.connect(host, port);
        eventLoop.run();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }

    std::cout << "Program finished." << std::endl;

    return 0;
}

