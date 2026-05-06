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
        src/libp2p_module_interface.h
        src/plugin.h
        src/plugin.cpp
        src/callbacks.cpp
        src/sync.cpp
        src/kademlia.cpp
        src/stream.cpp
        src/mix.cpp
        src/gossipsub.cpp
        src/service_discovery.cpp
    EXTERNAL_LIBS
        libp2p
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(libp2p_module_module_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# plugin.h includes <logos_api.h>, so consumers (tests, examples) need SDK headers
if(LOGOS_CPP_SDK_IS_SOURCE)
    target_include_directories(libp2p_module_module_plugin PUBLIC
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(libp2p_module_module_plugin PUBLIC
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
    )
endif()

add_subdirectory(examples)
