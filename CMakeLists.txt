cmake_minimum_required(VERSION 3.14)
project(NetPulse)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

cmake_policy(SET CMP0135 NEW)

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fexperimental-library)
    add_link_options(-fexperimental-library)
endif()

include_directories(include)

# 1. Fetch Google Test First
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# 2. Create your targets (Executables)
add_executable(netpulse src/main.cpp
        src/socket_wrapper.cpp
        include/socket_wrapper.h
        include/thread_safe_queue.h
        src/client_session.cpp
        include/client_session.h
        include/message.h)

add_executable(test_queue
        tests/test_thread_safe_queue.cpp)
target_link_libraries(test_queue gtest_main gtest pthread)

# 3. NOW apply Clang-Tidy (Because the targets finally exist!)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set_target_properties(netpulse test_queue PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found in PATH. Skipping static analysis.")
endif()