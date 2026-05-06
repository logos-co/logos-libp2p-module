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

enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Core Test)

function(libp2p_register_test source)
    get_filename_component(test_name ${source} NAME_WE)
    add_executable(${test_name} ${source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${test_name} PRIVATE
        libp2p_module_module_plugin
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

file(GLOB TEST_SOURCES tests/*.cpp)
foreach(source ${TEST_SOURCES})
    libp2p_register_test(${source})
endforeach()

add_subdirectory(examples)
