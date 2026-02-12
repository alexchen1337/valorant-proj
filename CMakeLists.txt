cmake_minimum_required(VERSION 3.20)
project(valorant-fatigue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

find_package(OpenSSL REQUIRED)

add_library(valorant_lib STATIC
    src/rate_limiter.cpp
    src/api_client.cpp
    src/cache.cpp
    src/session_detector.cpp
    src/analytics.cpp
    src/display.cpp
    src/env.cpp
)

target_include_directories(valorant_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps
)

target_compile_definitions(valorant_lib PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(valorant_lib PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    ftxui::screen
    ftxui::dom
    ftxui::component
)

add_executable(valorant-fatigue src/main.cpp)
target_link_libraries(valorant-fatigue PRIVATE valorant_lib)

enable_testing()
add_executable(valorant_tests
    tests/test_analytics.cpp
    tests/test_session_detector.cpp
    tests/test_env.cpp
)
target_link_libraries(valorant_tests PRIVATE valorant_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(valorant_tests)
