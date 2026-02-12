cmake_minimum_required(VERSION 3.20)
project(valorant-fatigue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)

add_library(valorant_lib STATIC
    src/rate_limiter.cpp
    src/api_client.cpp
    src/cache.cpp
    src/session_detector.cpp
    src/analytics.cpp
    src/display.cpp
)

target_include_directories(valorant_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps
)

target_compile_definitions(valorant_lib PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(valorant_lib PUBLIC OpenSSL::SSL OpenSSL::Crypto)

add_executable(valorant-fatigue src/main.cpp)
target_link_libraries(valorant-fatigue PRIVATE valorant_lib)
