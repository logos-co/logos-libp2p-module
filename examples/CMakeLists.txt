add_executable(example_kademlia
    kademlia.cpp
)

target_link_libraries(example_kademlia PRIVATE
    libp2p_module_module_plugin
    Qt6::Core
)

target_include_directories(example_kademlia PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
# Ensure runtime can find plugin + libp2p inside nix store
set_target_properties(example_kademlia PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/../lib"
)

# IMPORTANT — this installs into nix output
install(TARGETS example_kademlia
        RUNTIME DESTINATION examples)

