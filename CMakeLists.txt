# Logos Libp2p Module - Migrated CMakeLists.txt
# Simplified using logos-module-builder

cmake_minimum_required(VERSION 3.14)
project(Libp2pModulePlugin LANGUAGES CXX)

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
        src/libp2p_module_plugin.h
        src/libp2p_module_plugin.cpp
        src/libp2p_module_callbacks.cpp
        src/libp2p_module_sync.cpp
    EXTERNAL_LIBS
        libp2p
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Core Test)

logos_find_dependencies()

add_executable(test_libp2p_module
    tests/test_libp2p_module.cpp
)

target_include_directories(test_libp2p_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---- Logos SDK includes ----
if(LOGOS_CPP_SDK_IS_SOURCE)
    target_include_directories(test_libp2p_module PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(test_libp2p_module PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
    )
endif()

target_link_libraries(test_libp2p_module PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_libp2p_module COMMAND test_libp2p_module)

# sync test
add_executable(test_libp2p_module_sync
    tests/test_libp2p_module_sync.cpp
)

target_include_directories(test_libp2p_module_sync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(LOGOS_CPP_SDK_IS_SOURCE)
    target_include_directories(test_libp2p_module_sync PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(test_libp2p_module_sync PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
    )
endif()

target_link_libraries(test_libp2p_module_sync PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
    Qt6::Test
)

add_test(NAME test_libp2p_module_sync COMMAND test_libp2p_module_sync)
