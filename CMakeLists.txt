# Logos Libp2p Module - Migrated CMakeLists.txt
# Simplified using logos-module-builder

cmake_minimum_required(VERSION 3.14)
project(Libp2pModulePlugin LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include the Logos Module CMake helper
if(DEFINED ENV{LOGOS_MODULE_BUILDER_ROOT})
    include($ENV{LOGOS_MODULE_BUILDER_ROOT}/cmake/LogosModule.cmake)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LogosModule.cmake")
    include(cmake/LogosModule.cmake)
else()
    message(FATAL_ERROR "LogosModule.cmake not found")
endif()

# Define the module
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

target_include_directories(libp2p_module_module_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(APPLE)
    set_target_properties(libp2p_module_module_plugin PROPERTIES
        BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        INSTALL_RPATH "@loader_path;@loader_path/../lib"
    )
endif()

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Core Test)

logos_find_dependencies()

# Registers a single test source file as a CTest target.
# Each file in tests/ becomes its own executable named after the file stem.
function(libp2p_register_test source)
    get_filename_component(test_name ${source} NAME_WE)

    add_executable(${test_name} ${source})

    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(LOGOS_CPP_SDK_IS_SOURCE)
        target_include_directories(${test_name} PRIVATE
            ${LOGOS_CPP_SDK_ROOT}/cpp
            ${LOGOS_CPP_SDK_ROOT}/cpp/generated
        )
    else()
        target_include_directories(${test_name} PRIVATE
            ${LOGOS_CPP_SDK_ROOT}/include
            ${LOGOS_CPP_SDK_ROOT}/include/cpp
            ${LOGOS_CPP_SDK_ROOT}/include/core
        )
    endif()

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


# for examples
add_subdirectory(examples)
