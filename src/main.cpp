#include <iostream>
#include <thread>
#include <iostream>
#include <thread>
#include "EventLoop.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];

    EventLoop event_loop(host, port);

    // Set up the message callback
    event_loop.set_on_message([](const std::string& message) {
        std::cout << "Received: " << message << std::endl;
    });

    // Set up the error callback
    event_loop.set_on_error([](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    });

    // Try to connect synchronously
    if (event_loop.connect()) {
        std::cout << "Successfully connected!" << std::endl;
    } else {
        std::cerr << "Connection failed!" << std::endl;
        return 1;
    }

    // Wait until the user decides to stop
    std::string message;
    while (event_loop.is_connected()) {
        std::cout << "Enter message to send (or 'exit' to quit): ";
        std::getline(std::cin, message);
        if (message == "exit") {
            break;
        }
        event_loop.send(message);
    }

    return 0;
}

