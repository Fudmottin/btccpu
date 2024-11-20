#include <boost/asio.hpp>
#include <iostream>
#include <thread>

void timerHandler(const boost::system::error_code& ec) {
    if (!ec) {
        std::cout << "Timer expired!" << std::endl;
    }
}

int main() {
    // Create io_context
    boost::asio::io_context io_context;

    // Create a timer
    boost::asio::steady_timer timer(io_context, boost::asio::chrono::seconds(2));

    // Schedule an asynchronous wait
    timer.async_wait(&timerHandler);

    // Run the io_context
    std::cout << "Starting io_context.run()" << std::endl;
    io_context.run();
    std::cout << "io_context.run() has finished" << std::endl;

    return 0;
}

