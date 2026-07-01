cmake_minimum_required(VERSION 3.14)
project(Libp2pModuleTests LANGUAGES CXX)

include(LogosTest)

# Fast unit layer: header-inline logic plus the pure utils.cpp helpers it calls
# (e.g. decodeHex), links without libp2p.so (still needs the cbind header in
# ../lib for constants/types).

logos_test(
    NAME libp2p_module_unit_tests
    MODULE_SOURCES
        ../src/utils.cpp
    TEST_SOURCES
        main.cpp
        unit_config.cpp
        unit_metrics.cpp
        unit_sync.cpp
    EXTRA_INCLUDES
        ../lib
)

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
            ../src/utils.cpp
            ../src/plugin.cpp
            ../src/callbacks.cpp
            ../src/kademlia.cpp
            ../src/stream.cpp
            ../src/gossipsub.cpp
            ../src/service_discovery.cpp
            ../src/custom_handlers.cpp
            ../src/peerstore.cpp
            ../src/metrics.cpp
        TEST_SOURCES
            main.cpp
            sync.cpp
            config.cpp
            integration.cpp
            integration_gossipsub.cpp
            integration_kad.cpp
            integration_service_discovery.cpp
            custom_handlers.cpp
            integration_peerstore.cpp
            metrics.cpp
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
