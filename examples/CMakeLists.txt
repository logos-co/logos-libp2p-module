add_executable(example_kademlia
    kademlia.cpp
)

target_link_libraries(example_kademlia PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
)

target_include_directories(example_kademlia PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)
# Logos SDK includes
if(LOGOS_CPP_SDK_IS_SOURCE)
    target_include_directories(example_kademlia PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(example_kademlia PRIVATE
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
    )
endif()


# Ensure runtime can find plugin + libp2p inside nix store
set_target_properties(example_kademlia PROPERTIES
    BUILD_RPATH "${CMAKE_BINARY_DIR}/modules"
    INSTALL_RPATH "$ORIGIN/../lib"
)

# IMPORTANT — this installs into nix output
install(TARGETS example_kademlia
        RUNTIME DESTINATION examples)





