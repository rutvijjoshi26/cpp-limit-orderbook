cmake_minimum_required(VERSION 3.15)
project(cpp_limit_orderbook CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_library(orderbook src/orderbook.cpp)
target_include_directories(orderbook PUBLIC include)

add_executable(orderbook_test tests/orderbook_test.cpp)
target_link_libraries(orderbook_test PRIVATE orderbook)
target_include_directories(orderbook_test PRIVATE include tests)

add_executable(bench src/bench.cpp)
target_link_libraries(bench PRIVATE orderbook)
target_include_directories(bench PRIVATE include)

enable_testing()
add_test(NAME orderbook_test COMMAND orderbook_test)
