#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <string>
#include <deque>

class EventLoop {
public:
    EventLoop(const std::string& host, const std::string& port)
        : resolver_(io_context_),
          socket_(io_context_),
          host_(host),
          port_(port) {}

    void connect() {
        try {
            // Resolve the host and port
            auto endpoints = resolver_.resolve(host_, port_);

            // Establish a connection
            boost::asio::connect(socket_, endpoints);

            std::cout << "Connected to " << host_ << ":" << port_ << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Connection failed: " << e.what() << std::endl;
            throw;
        }
    }

    void send(const std::string& message) {
        try {
            boost::asio::write(socket_, boost::asio::buffer(message));
            std::cout << "Message sent: " << message << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to send message: " << e.what() << std::endl;
            throw;
        }
    }

    void receive() {
        try {
            boost::asio::async_read_until(
                socket_,
                boost::asio::dynamic_buffer(buffer_),
                "\n",
                [this](boost::system::error_code ec, std::size_t length) {
                    if (!ec) {
                        std::string message = buffer_.substr(0, length - 1); // Exclude the newline
                        buffer_.erase(0, length); // Remove processed data
                        handle_message(message);
                        // Continue receiving more data
                        receive();
                    } else {
                        if (ec != boost::asio::error::eof) {
                            std::cerr << "Receive error: " << ec.message() << std::endl;
                        }
                    }
                });
        } catch (const std::exception& e) {
            std::cerr << "Error in receive: " << e.what() << std::endl;
        }
    }

    void run() {
        io_context_.run();
    }

protected:
    virtual void handle_message(const std::string& message) {
        std::cout << "Received message: " << message << std::endl;
    }

private:
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    std::string host_;
    std::string port_;
    std::string buffer_; // Buffer for incoming data
};

int main() {
    try {
        std::string host = "10.0.1.210";  // Replace with the target host
        std::string port = "3333";        // Replace with the target port

        EventLoop eventLoop(host, port);
        eventLoop.connect();

        // Start receiving data
        eventLoop.receive();

        // Run the event loop
        eventLoop.run();
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
    }

    return 0;
}

