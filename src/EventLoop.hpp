#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <boost/asio.hpp>
#include <functional>
#include <iostream>
#include <string>
#include <memory>
#include <mutex>
#include <queue>

class EventLoop {
public:
    using message_callback_t = std::function<void(const std::string&)>;
    using error_callback_t = std::function<void(const std::string&)>;

    EventLoop(const std::string& host, const std::string& port);
    ~EventLoop();

    void start();
    void stop();

    bool connect();  // Synchronous connection method
    bool is_connected() const;  // Check connection status

    bool send(const std::string& message);

    void async_send(const std::string& message);
    void set_on_message(message_callback_t callback);
    void set_on_error(error_callback_t callback);

private:
    void start_receive();
    void handle_message(const std::string& message);

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::streambuf buffer_;

    std::string host_;
    std::string port_;

    std::mutex send_mutex_;
    message_callback_t on_message_;
    error_callback_t on_error_;
};

#endif // EVENT_LOOP_H

