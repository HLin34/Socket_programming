cmake_minimum_required(VERSION 3.16)
project(SocketProgramming LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
add_executable(server server.cpp student_directory.cpp dns.cpp)
target_link_libraries(server PRIVATE Threads::Threads)
add_executable(client client.cpp)
foreach(t server client)
  target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(NAME tcp_integration COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration.py --server $<TARGET_FILE:server>)
set_tests_properties(tcp_integration PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
add_test(NAME tcp_concurrency
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_concurrency.py
            --server $<TARGET_FILE:server>
)

set_tests_properties(tcp_concurrency PROPERTIES
    TIMEOUT 30
    RUN_SERIAL TRUE
)