file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS *.cpp)
# mix.cpp temporarily disabled — extracted to separate repo, no cbindings yet
list(FILTER EXAMPLE_SOURCES EXCLUDE REGEX "/mix\\.cpp$")

foreach(example_src ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${example_src} NAME_WE)
    set(target_name example_${example_name})

    add_executable(${target_name} ${example_src})

    target_link_libraries(${target_name} PRIVATE
        libp2p_module_module_plugin
    )

    target_include_directories(${target_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include
    )

    set_target_properties(${target_name} PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/modules"
        INSTALL_RPATH "$ORIGIN/../lib"
    )

    install(TARGETS ${target_name}
            RUNTIME DESTINATION examples)
endforeach()
