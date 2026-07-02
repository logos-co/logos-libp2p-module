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
        src/libp2p_consts.h
        src/utils.h
        src/utils.cpp
        src/plugin.cpp
        src/callbacks.cpp
        src/kademlia.cpp
        src/stream.cpp
        src/gossipsub.cpp
        src/service_discovery.cpp
        src/custom_handlers.cpp
        src/peerstore.cpp
        src/metrics.cpp
    EXTERNAL_LIBS
        libp2p
    # The nim-ffi header (lib/libp2p.h) includes <tinycbor/cbor.h> and its CBOR
    # marshalling calls into TinyCBOR (provided as a nix.packages.runtime dep).
    LINK_LIBRARIES
        tinycbor
)

add_subdirectory(examples)
