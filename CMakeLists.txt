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
    EXTERNAL_LIBS
        libp2p
)
