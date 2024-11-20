#include "EventLoop.hpp"

EventLoop::EventLoop(const std::string& host, const std::string& port)
    : resolver_(io_context_), socket_(io_context_), host_(host), port_(port) {}

EventLoop::~EventLoop() { stop(); }

void EventLoop::start() {
    io_context_.run();  // Begin the event loop to handle asynchronous operations
}

void EventLoop::stop() {
    io_context_.stop();
    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.close(ec);
        if (ec && on_error_) {
            on_error_("Socket close error: " + ec.message());
        }
    }
}

bool EventLoop::connect() {
    try {
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver_.resolve(host_, port_);
        boost::asio::connect(socket_, endpoints);  // Synchronously connect to the server
        if (socket_.is_open()) {
            start_receive();  // Start receiving after successful connection
            return true;
        }
    } catch (const boost::system::system_error& e) {
        if (on_error_) {
            on_error_("Connection failed: " + std::string(e.what()));
        }
    }
    return false;
}

bool EventLoop::is_connected() const {
    return socket_.is_open();  // Check if the socket is still open
}

bool EventLoop::send(const std::string& message) {
    try {
        boost::asio::write(socket_, boost::asio::buffer(message + "\n"));
    }
    catch (const boost::system::system_error& e) {
        std::cerr << "Error while sending: " << std::string(e.what());
        return false;
    }

    return true;
}

void EventLoop::async_send(const std::string& message) {
    if (!socket_.is_open()) {
        if (on_error_) on_error_("Send error: Socket not open");
        return;
    }

    boost::asio::async_write(
        socket_, boost::asio::buffer(message + "\n"),
        [this, message](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
            if (ec) {
                if (on_error_) on_error_("Send error: " + ec.message());
                return;
            }
            // Log sent message confirmation
            std::cout << "Sent: " << message << std::endl;

            // After sending the message, continue to receive responses
            start_receive();
        });
}

void EventLoop::start_receive() {
    if (!socket_.is_open()) {
        if (on_error_) on_error_("Receive error: Socket not open");
        return;
    }

    boost::asio::async_read_until(
        socket_, buffer_, '\n',
        [this](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                if (ec == boost::asio::error::operation_aborted) {
                    return; // Normal during shutdown
                }
                if (on_error_) on_error_("Receive error: " + ec.message());
                return;
            }

            // Process the message after receiving
            std::istream is(&buffer_);
            std::string message;
            std::getline(is, message);

            std::cout << "Received: " << message << std::endl;

            // Pass to the callback for processing
            if (on_message_) {
                on_message_(message);
            }

            // Continue to listen for further messages
            start_receive();
        });
}

void EventLoop::handle_message(const std::string& message) {
    if (on_message_) {
        on_message_(message);
    }
}

void EventLoop::set_on_message(message_callback_t callback) {
    on_message_ = std::move(callback);
}

void EventLoop::set_on_error(error_callback_t callback) {
    on_error_ = std::move(callback);
}

