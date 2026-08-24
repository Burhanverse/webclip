cmake_minimum_required(VERSION 3.16)
project(webclip_sync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Dependencies
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

set(SOURCES
    src/main.cpp
    src/clipboard/clipboard.cpp
    src/net/http_client.cpp
    src/net/sse_parser.cpp
    src/sync/sync_manager.cpp
)

set(HEADERS
    src/clipboard/clipboard.hpp
    src/net/http_client.hpp
    src/net/sse_parser.hpp
    src/sync/sync_manager.hpp
    src/util/json.hpp
    src/util/cli.hpp
)

add_executable(webclip_sync ${SOURCES} ${HEADERS})

target_include_directories(webclip_sync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(webclip_sync PRIVATE
    Threads::Threads
    ${CURL_LIBRARIES}
)

if(WIN32)
    target_link_libraries(webclip_sync PRIVATE
        ws2_32
        user32
    )
endif()

# Compiler warnings and optimization flags
if(MSVC)
    target_compile_options(webclip_sync PRIVATE /W4 /O2 /permissive-)
else()
    target_compile_options(webclip_sync PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

install(TARGETS webclip_sync RUNTIME DESTINATION bin)
