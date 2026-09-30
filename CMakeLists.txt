cmake_minimum_required(VERSION 3.22)
project(DustwireMP VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

include(FetchContent)

# Pin ENet to a known upstream commit for reproducible configuration.
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG 5a9c537fd464b3c6d3c55e1d3bd47588faf71b42
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(enet)

add_library(dustwire_shared STATIC
    shared/net/EnetTransport.cpp
    shared/net/UdpTransport.cpp
)
target_include_directories(dustwire_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dustwire_shared PUBLIC enet)

if (WIN32)
    target_link_libraries(dustwire_shared PUBLIC ws2_32 winmm)
endif()

add_library(dustwire_client_networking STATIC
    client/client-networking/ClientNetwork.cpp
)
target_include_directories(dustwire_client_networking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dustwire_client_networking PUBLIC dustwire_shared)

add_library(dustwire_client_game STATIC
    client/client-game/GameBridge.cpp
)
target_include_directories(dustwire_client_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(dustwire_client_cef STATIC
    client/client-cef/CEFBridge.cpp
)
target_include_directories(dustwire_client_cef PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DustwireMPClient
    client/client-main/main.cpp
)
target_link_libraries(DustwireMPClient PRIVATE
    dustwire_client_networking
    dustwire_client_game
    dustwire_client_cef
)

add_library(dustwire_server_networking STATIC
    server/server-networking/ServerNetwork.cpp
)
target_include_directories(dustwire_server_networking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dustwire_server_networking PUBLIC dustwire_shared)

add_executable(DustwireMPServer
    server/server-main/main.cpp
)
target_link_libraries(DustwireMPServer PRIVATE dustwire_server_networking)

enable_testing()

add_executable(dustwire_protocol_smoke
    tests/protocol_smoke.cpp
)
target_include_directories(dustwire_protocol_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME dustwire_protocol_smoke COMMAND dustwire_protocol_smoke)
