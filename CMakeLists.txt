cmake_minimum_required(VERSION 3.16)
project(ome)

set(CMAKE_CXX_STANDARD 20)

# Add Liquibook as submodule
add_subdirectory(src/3rdParty/order-matching-engine)

# Your app
add_executable(ome src/main.cpp)
add_executable(benchmark src/benchmark.cpp)

# Link with Liquibook
target_link_libraries(ome PRIVATE liquibook)
target_link_libraries(benchmark PRIVATE liquibook)

