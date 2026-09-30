cmake_minimum_required(VERSION 3.22)
project(DustwireMP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

add_library(dustwire_shared STATIC
    shared/net/UdpTransport.cpp
)
target_include_directories(dustwire_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if (WIN32)
    target_link_libraries(dustwire_shared PUBLIC ws2_32)
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
