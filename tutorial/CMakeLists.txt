# Tutorials for logos-libp2p-module
# Build with: cmake -B build -S . && cmake --build build -j
# Run with:   ./build/tutorial_<name>

file(GLOB TUTORIAL_SOURCES CONFIGURE_DEPENDS *.cpp)

foreach(tutorial_src ${TUTORIAL_SOURCES})
    get_filename_component(tutorial_name ${tutorial_src} NAME_WE)

    add_executable(${tutorial_name} ${tutorial_src})

    target_link_libraries(${tutorial_name} PRIVATE
        libp2p_module_module_plugin
    )

    target_include_directories(${tutorial_name} PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/lib
    )

    set_target_properties(${tutorial_name} PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/modules"
        INSTALL_RPATH "$ORIGIN/../lib"
    )

    install(TARGETS ${tutorial_name}
            RUNTIME DESTINATION tutorial)
endforeach()
