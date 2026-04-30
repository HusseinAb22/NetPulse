cmake_minimum_required(VERSION 3.14)
project(NetPulse)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_policy(SET CMP0135 NEW)


include_directories(include)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found in PATH. Skipping static analysis during build.")
endif()


add_executable(netpulse src/main.cpp
        src/socket_wrapper.cpp
        include/socket_wrapper.h
        include/thread_safe_queue.h
        src/client_session.cpp
        include/client_session.h
        include/message.h)

