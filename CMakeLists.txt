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
        include/message.h
        src/protocol.cpp
        include/protocol.h
        include/line_framer.h
        src/chat_room.cpp
        include/chat_room.h
        src/room_manager.cpp
        include/room_manager.h
        src/nickname_registry.cpp
        include/nickname_registry.h
        src/dispatcher.cpp
        include/dispatcher.h
        src/server.cpp
        include/server.h)

add_executable(tests
        tests/test_thread_safe_queue.cpp
        tests/test_protocol.cpp
        src/protocol.cpp
        tests/test_line_framer.cpp
        src/chat_room.cpp
        include/chat_room.h
        include/client_session.h
        src/client_session.cpp
        include/socket_wrapper.h
        src/socket_wrapper.cpp
        tests/test_chat_room.cpp
        src/room_manager.cpp
        include/room_manager.h
        tests/test_helpers.h
        tests/test_room_manager.cpp
        src/nickname_registry.cpp
        include/nickname_registry.h
        tests/test_nickname_registry.cpp
        src/dispatcher.cpp
        include/dispatcher.h
        tests/test_dispatcher.cpp
        src/server.cpp
        include/server.h
        tests/test_server.cpp)
target_link_libraries(tests gtest_main gtest pthread)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(netpulse PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wswitch-enum
            -Wno-missing-field-initializers)
    target_compile_options(tests    PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wswitch-enum
            -Wno-missing-field-initializers)
endif()

# 3. NOW apply Clang-Tidy (Because the targets finally exist!)
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    set_target_properties(netpulse tests PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
else()
    message(WARNING "clang-tidy not found in PATH. Skipping static analysis.")
endif()