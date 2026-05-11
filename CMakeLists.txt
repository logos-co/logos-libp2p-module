cmake_minimum_required(VERSION 3.14)
project(Libp2pModulePlugin LANGUAGES CXX)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

logos_module(
    NAME libp2p_module
    SOURCES
        src/plugin.h
        src/plugin.cpp
        src/callbacks.cpp
        src/kademlia.cpp
        src/stream.cpp
        # src/mix.cpp  # temporarily disabled — extracted to separate repo, no cbindings yet
        src/gossipsub.cpp
        src/service_discovery.cpp
        src/peerstore.cpp
    EXTERNAL_LIBS
        libp2p
)

add_subdirectory(examples)
