#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>

int main() {
    // Get the current time in UTC
    auto now = std::chrono::system_clock::now();
    
    // Convert to a time_t (seconds since epoch)
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    // Convert time_t to UTC tm structure
    std::tm *utc_tm = std::gmtime(&now_c);
    
    // Format the time in UTC
    std::cout << "Current UTC time: " << std::put_time(utc_tm, "%Y-%m-%d %H:%M:%S") << " UTC" << std::endl;
    
    // Print the epoch time in seconds
    std::cout << "Seconds since epoch: " << now_c << " seconds" << std::endl;

    return 0;
}

