cmake_minimum_required(VERSION 3.14)
project(Libp2pModulePlugin LANGUAGES CXX)

include(CheckCXXSourceCompiles)

if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found. Set LOGOS_MODULE_BUILDER_ROOT.")
endif()

logos_module(
    NAME libp2p_module
    SOURCES
        src/plugin.h
        src/utils.h
        src/utils.cpp
        src/plugin.cpp
        src/callbacks.cpp
        src/kademlia.cpp
        src/stream.cpp
        src/protocol_bridge.cpp
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

set(CMAKE_REQUIRED_INCLUDES ${LIBP2P_INCLUDE_DIR})
check_cxx_source_compiles("
    #include <libp2p.h>
    int main() {
        auto fn = &libp2p_ctx_set_log_level;
        (void)fn;
        return 0;
    }
" HAVE_LIBP2P_CTX_SET_LOG_LEVEL)

if(HAVE_LIBP2P_CTX_SET_LOG_LEVEL)
    target_compile_definitions(libp2p_module_module_plugin
        PRIVATE HAVE_LIBP2P_CTX_SET_LOG_LEVEL)
endif()

add_subdirectory(tutorial)
