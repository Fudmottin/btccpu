#include <boost/asio.hpp>
#include <iostream>
#include <string>

using boost::asio::ip::tcp;

int main() {
    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::socket socket(io_context);

    auto endpoints = resolver.resolve("10.0.1.210", "7");
    boost::asio::connect(socket, endpoints);

    std::string message = "Hello";
    boost::asio::write(socket, boost::asio::buffer(message + "\n"));

    char buf[128];
    size_t len = socket.read_some(boost::asio::buffer(buf));
    std::cout << "Received: " << std::string(buf, len) << std::endl;

    return 0;
}

