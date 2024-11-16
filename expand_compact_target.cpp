#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <boost/multiprecision/cpp_int.hpp>

using namespace boost::multiprecision;

// Function to expand compact target format to uint256_t
uint256_t expandCompactTarget(uint32_t compact) {
    uint32_t exponent = (compact >> 24) & 0xff;
    uint32_t coefficient = compact & 0x007fffff;

    uint256_t target;
    if (exponent <= 3) {
        target = coefficient >> (8 * (3 - exponent));
    } else {
        target = uint256_t(coefficient) * (uint256_t(1) << (8 * (exponent - 3)));
    }
    return target;
}

// Main function to accept command line argument and output expanded target in hex
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <32-bit unsigned integer>" << std::endl;
        return 1;
    }

    // Convert command line argument to uint32_t
    uint32_t compact;
    try {
        compact = std::stoul(argv[1], nullptr, 16);  // read as hex
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid input. Please enter a valid 32-bit unsigned integer in hex." << std::endl;
        return 1;
    }

    // Call expandCompactTarget and get result
    uint256_t target = expandCompactTarget(compact);

    // Convert result to a hexadecimal string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << target;

    // Print the full expanded target in hex format, padded to 64 characters
    std::string hex_str = oss.str();
    if (hex_str.size() < 64) {
        hex_str.insert(0, 64 - hex_str.size(), '0');  // pad with leading zeros if necessary
    }

    std::cout << "Expanded target (hex): " << hex_str << std::endl;

    return 0;
}

