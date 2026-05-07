cmake_minimum_required(VERSION 3.14)
project(Libp2pModuleTests LANGUAGES CXX)

include(LogosTest)

# Integration tests (real libp2p library)

find_library(LIBP2P_PATH
    NAMES libp2p.so libp2p.dylib
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    NO_DEFAULT_PATH)

if(LIBP2P_PATH)
    message(STATUS "[Libp2pTests] libp2p found: ${LIBP2P_PATH} — building integration tests")

    logos_test(
        NAME libp2p_module_tests
        MODULE_SOURCES
            ../src/plugin.cpp
            ../src/callbacks.cpp
            ../src/sync.cpp
            ../src/kademlia.cpp
            ../src/stream.cpp
            ../src/mix.cpp
            ../src/gossipsub.cpp
            ../src/service_discovery.cpp
        TEST_SOURCES
            main.cpp
            sync.cpp
            config.cpp
            integration.cpp
            integration_gossipsub.cpp
            integration_kad.cpp
            integration_mix.cpp
            integration_service_discovery.cpp
        EXTRA_INCLUDES
            ../lib
        EXTRA_LINK_LIBS
            ${LIBP2P_PATH}
    )

    get_filename_component(LIBP2P_DIR "${LIBP2P_PATH}" DIRECTORY)
    set_target_properties(libp2p_module_tests PROPERTIES
        BUILD_RPATH "${LIBP2P_DIR}"
    )
else()
    message(STATUS "[Libp2pTests] libp2p not found in ../lib — skipping integration tests")
endif()
