#include <iostream>
#include <thread>
#include <iostream>
#include <thread>
#include "EventLoop.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>\n";
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];

    try {
        EventLoop event_loop(host, port);

        // Set handlers
        event_loop.set_on_message([](const std::string& message) {
            std::cout << "Received: " << message << std::endl;
        });

        event_loop.set_on_error([](const std::string& error) {
            std::cerr << "Error: " << error << std::endl;
        });

        std::thread event_thread([&event_loop]() {
            event_loop.start();
        });

        std::cout << "Press Enter to send a test message...\n";
        std::cin.get();

        event_loop.async_send("Hello, server!");

        std::cout << "Press Enter to stop...\n";
        std::cin.get();

        event_loop.stop();
        event_thread.join();
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
    }

    return 0;
}
