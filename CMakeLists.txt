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
        src/plugin.h

        src/plugin.cpp
        src/callbacks.cpp
        src/sync.cpp
        src/kademlia.cpp
        src/stream.cpp
        src/mix.cpp
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

add_executable(async
    tests/async.cpp
)

target_include_directories(async PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ---- Logos SDK includes ----
if(LOGOS_CPP_SDK_IS_SOURCE)
    target_include_directories(async PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(async PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
    )
endif()

target_link_libraries(async PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
    Qt6::Test
)

add_test(NAME async COMMAND async)

# sync test
add_executable(sync
    tests/sync.cpp
)

target_include_directories(sync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(LOGOS_CPP_SDK_IS_SOURCE)
    target_include_directories(sync PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(sync PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
    )
endif()

target_link_libraries(sync PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
    Qt6::Test
)

add_test(NAME sync COMMAND sync)



add_executable(integration
    tests/integration.cpp
)

target_include_directories(integration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(integration PRIVATE
    ${LOGOS_CPP_SDK_ROOT}/include
    ${LOGOS_CPP_SDK_ROOT}/include/cpp
    ${LOGOS_CPP_SDK_ROOT}/include/core
)

target_link_libraries(integration PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
    Qt6::Test
)

add_test(NAME integration COMMAND integration)


# for examples
add_subdirectory(examples)
