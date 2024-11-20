#include "EventLoop.hpp"

EventLoop::EventLoop(const std::string& host, const std::string& port)
    : resolver_(io_context_), socket_(io_context_), host_(host), port_(port) {}

void EventLoop::start() {
    do_connect();
    io_context_.run();
}

void EventLoop::stop() {
    boost::asio::post(io_context_, [this]() {
        if (socket_.is_open()) {
            boost::system::error_code ec;
            socket_.close(ec);
            if (ec && on_error_) {
                on_error_("Socket close error: " + ec.message());
            }
        }
    });
    io_context_.stop();
}

void EventLoop::async_send(const std::string& message) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!socket_.is_open()) {
        if (on_error_) on_error_("Send error: Socket not open");
        return;
    }

    boost::asio::async_write(
        socket_, boost::asio::buffer(message + "\n"),
        [this](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
            if (ec && on_error_) {
                on_error_("Send error: " + ec.message());
            }
        });
}

void EventLoop::set_on_message(message_callback_t callback) {
    on_message_ = std::move(callback);
}

void EventLoop::set_on_error(error_callback_t callback) {
    on_error_ = std::move(callback);
}

void EventLoop::do_connect() {
    resolver_.async_resolve(
        host_, port_,
        [this](boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
            if (ec) {
                if (on_error_) on_error_("Resolve error: " + ec.message());
                return;
            }

            boost::asio::async_connect(
                socket_, results,
                [this](boost::system::error_code ec, const boost::asio::ip::tcp::endpoint&) {
                    if (ec) {
                        if (on_error_) on_error_("Connect error: " + ec.message());
                        return;
                    }

                    if (on_message_) {
                        on_message_("Connected to " + host_ + ":" + port_);
                    }

                    do_receive();
                });
        });
}

void EventLoop::do_receive() {
    boost::asio::async_read_until(
        socket_, buffer_, '\n',
        [this](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted && on_error_) {
                    on_error_("Receive error: " + ec.message());
                }
                return;
            }

            std::istream is(&buffer_);
            std::string message;
            std::getline(is, message);

            if (!message.empty() && on_message_) {
                on_message_(message);
            }

            do_receive();
        });
}

