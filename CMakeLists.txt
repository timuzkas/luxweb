cmake_minimum_required(VERSION 3.24)

project(luxweb VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(GNUInstallDirs)

option(LUXWEB_BUILD_EXAMPLES "Build example applications" ON)
option(LUXWEB_BUILD_TESTS "Build tests" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)
pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG asio-1-30-2
)

FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.2.1.2
)

FetchContent_Declare(
  inja
  GIT_REPOSITORY https://github.com/pantor/inja.git
  GIT_TAG v3.4.0
)

FetchContent_GetProperties(asio)
if(NOT asio_POPULATED)
  FetchContent_Populate(asio)
endif()
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "Asio include directory" FORCE)

set(BUILD_TESTING OFF CACHE BOOL "Disable dependency tests" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "Disable dependency benchmarks" FORCE)
set(INJA_BUILD_TESTS OFF CACHE BOOL "Disable Inja tests" FORCE)
set(INJA_INSTALL OFF CACHE BOOL "Disable Inja install rules" FORCE)
FetchContent_MakeAvailable(Crow)

FetchContent_GetProperties(inja)
if(NOT inja_POPULATED)
  FetchContent_Populate(inja)
endif()

if(NOT TARGET nlohmann_json)
  add_library(nlohmann_json INTERFACE)
  target_include_directories(nlohmann_json INTERFACE "${inja_SOURCE_DIR}/third_party/include")
  target_compile_features(nlohmann_json INTERFACE cxx_std_17)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

if(NOT TARGET inja)
  add_library(inja INTERFACE)
  target_include_directories(inja INTERFACE "${inja_SOURCE_DIR}/include")
  target_link_libraries(inja INTERFACE nlohmann_json::nlohmann_json)
  target_compile_features(inja INTERFACE cxx_std_17)
endif()
if(NOT TARGET pantor::inja)
  add_library(pantor::inja ALIAS inja)
endif()

add_library(luxweb_core
  src/luxweb_core/app.cpp
  src/luxweb_core/auth.cpp
  src/luxweb_core/context.cpp
  src/luxweb_core/crypto.cpp
  src/luxweb_core/dev_reload.cpp
  src/luxweb_core/http.cpp
  src/luxweb_core/style.cpp
  src/luxweb_core/template_engine.cpp
  src/luxweb_core/user_store.cpp
)

add_library(luxweb::core ALIAS luxweb_core)

target_include_directories(luxweb_core
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(luxweb_core
  PUBLIC
    Crow::Crow
    nlohmann_json::nlohmann_json
    pantor::inja
    PkgConfig::SQLITE3
    OpenSSL::Crypto
    PkgConfig::FMT
    PkgConfig::SPDLOG
    Threads::Threads
)

target_compile_options(luxweb_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(luxweb src/luxweb_cli/main.cpp)
target_link_libraries(luxweb PRIVATE luxweb::core)
target_compile_definitions(luxweb PRIVATE LUXWEB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_compile_options(luxweb PRIVATE -Wall -Wextra -Wpedantic)

if(LUXWEB_BUILD_EXAMPLES)
  add_executable(luxweb_starter examples/starter/src/main.cpp)
  target_link_libraries(luxweb_starter PRIVATE luxweb::core)
  target_compile_options(luxweb_starter PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LUXWEB_BUILD_TESTS)
  enable_testing()
  add_executable(luxweb_tests tests/luxweb_tests.cpp)
  target_link_libraries(luxweb_tests PRIVATE luxweb::core)
  target_compile_definitions(luxweb_tests PRIVATE LUXWEB_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  target_compile_options(luxweb_tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME luxweb_tests COMMAND luxweb_tests)
endif()
