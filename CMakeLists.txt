cmake_minimum_required(VERSION 3.14)
project(publisher CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)


add_library(publisher_core STATIC
    src/trust.cpp
    src/mailbox.cpp
    src/store.cpp
    src/service.cpp
    src/protocol.cpp
)
target_include_directories(publisher_core PUBLIC include)
target_link_libraries(publisher_core PUBLIC Threads::Threads)

add_executable(publisher_server src/server.cpp)
target_link_libraries(publisher_server PRIVATE publisher_core)

add_executable(publisher_client src/client.cpp)
target_link_libraries(publisher_client PRIVATE publisher_core)

option(IZDANIE_BUILD_TESTS "Собирать unit-тесты" ON)

if(IZDANIE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(publisher_tests
        tests/test_trust.cpp
        tests/test_mailbox.cpp
        tests/test_store.cpp
        tests/test_protocol.cpp
        tests/test_service.cpp
    )
    target_link_libraries(publisher_tests PRIVATE publisher_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(publisher_tests)
endif()