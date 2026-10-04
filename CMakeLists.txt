cmake_minimum_required(VERSION 3.20)
project(storeforge_session_service VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)
find_package(Threads REQUIRED)

add_library(session_core
    src/boost_json.cpp
    src/config.cpp
    src/redis_client.cpp
    src/session_store.cpp
    src/session_service.cpp
    src/request_router.cpp
    src/http_server.cpp
)

target_include_directories(session_core
    PUBLIC include
    PRIVATE ${HIREDIS_INCLUDE_DIRS}
)

target_compile_definitions(session_core PUBLIC BOOST_ERROR_CODE_HEADER_ONLY)
target_link_libraries(session_core PUBLIC ${HIREDIS_LIBRARIES} Threads::Threads)
target_compile_options(session_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(session-service src/main.cpp)
target_link_libraries(session-service PRIVATE session_core)

enable_testing()
add_executable(session-tests tests/session_service_tests.cpp)
target_link_libraries(session-tests PRIVATE session_core)
add_test(NAME session-tests COMMAND session-tests)
