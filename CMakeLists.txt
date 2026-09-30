cmake_minimum_required(VERSION 4.2)
project(DustwireMP VERSION 0.3.0 LANGUAGES C CXX)

if (NOT WIN32)
    message(FATAL_ERROR "DustwireMP client/launcher currently requires Windows.")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

include(FetchContent)

# Pin ENet to an upstream commit for reproducible configuration.
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG 5a9c537fd464b3c6d3c55e1d3bd47588faf71b42
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(enet)

add_library(dustwire_logging STATIC
    shared/logging/Logger.cpp
)
target_include_directories(
    dustwire_logging
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(dustwire_shared STATIC
    shared/net/EnetTransport.cpp
)

target_include_directories(
    dustwire_shared
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${enet_SOURCE_DIR}/include
)

target_link_libraries(
    dustwire_shared
    PUBLIC enet
)

target_compile_definitions(
    dustwire_shared
    PRIVATE WIN32_LEAN_AND_MEAN
)

target_link_libraries(
    dustwire_shared
    PUBLIC
        ws2_32
        winmm
)

add_library(dustwire_client_networking STATIC
    client/client-networking/ClientNetwork.cpp
)
target_include_directories(
    dustwire_client_networking
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    dustwire_client_networking
    PUBLIC dustwire_shared
)

add_library(dustwire_client_game STATIC
    client/client-game/GameBridge.cpp
)
target_include_directories(
    dustwire_client_game
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(dustwire_client_cef STATIC
    client/client-cef/CEFBridge.cpp
)
target_include_directories(
    dustwire_client_cef
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# Console client used for protocol/network development.
add_executable(DustwireMPClient
    client/client-main/main.cpp
)
target_link_libraries(
    DustwireMPClient
    PRIVATE
        dustwire_client_networking
        dustwire_client_game
        dustwire_client_cef
)

# Actual injected client module.
add_library(DustwireMPClientModule SHARED
    client/client-main/dllmain.cpp
    client/client-main/ClientMain.cpp
    client/client-main/GameTick.cpp
)

set_target_properties(
    DustwireMPClientModule
    PROPERTIES
        OUTPUT_NAME "DustwireMPClientModule"
)

target_include_directories(
    DustwireMPClientModule
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    DustwireMPClientModule
    PRIVATE
        dustwire_logging
        dustwire_client_networking
        dustwire_client_game
        dustwire_client_cef
)

# Launcher responsible for locating/verifying RDR.exe and injecting the module.
add_executable(DustwireMPLauncher
    client/launcher/main.cpp
    client/launcher/ProcessLocator.cpp
    client/launcher/Injector.cpp
    client/launcher/GameBuild.cpp
    client/launcher/LauncherConfig.cpp
)

target_include_directories(
    DustwireMPLauncher
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    DustwireMPLauncher
    PRIVATE
        dustwire_logging
        kernel32
)

add_custom_command(
    TARGET DustwireMPLauncher
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:DustwireMPLauncher>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config/launcher.ini"
        "$<TARGET_FILE_DIR:DustwireMPLauncher>/config/launcher.ini"
)

add_library(dustwire_server_networking STATIC
    server/server-networking/ServerNetwork.cpp
)
target_include_directories(
    dustwire_server_networking
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    dustwire_server_networking
    PUBLIC dustwire_shared
)

add_executable(DustwireMPServer
    server/server-main/main.cpp
)
target_link_libraries(
    DustwireMPServer
    PRIVATE dustwire_server_networking
)

enable_testing()

add_executable(dustwire_protocol_smoke
    tests/protocol_smoke.cpp
)
target_include_directories(
    dustwire_protocol_smoke
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(
    NAME dustwire_protocol_smoke
    COMMAND dustwire_protocol_smoke
)

add_executable(dustwire_enet_loopback
    tests/enet_loopback.cpp
)
target_include_directories(
    dustwire_enet_loopback
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(
    dustwire_enet_loopback
    PRIVATE dustwire_shared
)
add_test(
    NAME dustwire_enet_loopback
    COMMAND dustwire_enet_loopback
)
