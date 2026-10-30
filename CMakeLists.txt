cmake_minimum_required(VERSION 3.16)

project(SocketProgramming LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(server
    server.cpp
    student_directory.cpp
    dns.cpp
)

add_executable(client
    client.cpp
)

target_compile_options(server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)