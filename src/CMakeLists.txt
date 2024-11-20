# Add the executable
add_executable(main main.cpp EventLoop.cpp)

# Include directories
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link Boost libraries
find_package(Boost REQUIRED COMPONENTS system thread)
target_link_libraries(main PRIVATE Boost::boost Boost::system)

