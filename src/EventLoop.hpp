#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <mutex>

class EventLoop {
public:
    using message_callback_t = std::function<void(const std::string&)>;
    using error_callback_t = std::function<void(const std::string&)>;

    EventLoop(const std::string& host, const std::string& port);

    void start();
    void stop();

    void async_send(const std::string& message);
    void set_on_message(message_callback_t callback);
    void set_on_error(error_callback_t callback);

private:
    void do_connect();
    void do_receive();

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

