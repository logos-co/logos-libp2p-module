# Tutorials for logos-libp2p-module
# Build with: cmake -B build -S . && cmake --build build -j
# Run with:   ./build/tutorial_<name>

file(GLOB TUTORIAL_SOURCES CONFIGURE_DEPENDS *.cpp)

find_package(nlohmann_json REQUIRED)
find_path(LIBP2P_INCLUDE_DIR
    NAMES libp2p.h
    PATHS ${PROJECT_SOURCE_DIR}/lib
)
if(NOT LIBP2P_INCLUDE_DIR)
    message(FATAL_ERROR "libp2p.h not found. Build inside the Nix dev shell or provide libp2p headers.")
endif()

find_path(LOGOS_CPP_SDK_INCLUDE_DIR
    NAMES logos_json.h logos_result.h
    PATHS ENV LOGOS_CPP_SDK_ROOT
    PATH_SUFFIXES include
)
if(NOT LOGOS_CPP_SDK_INCLUDE_DIR)
    message(FATAL_ERROR "Logos C++ SDK headers not found. Build inside the Nix dev shell or provide LOGOS_CPP_SDK_ROOT.")
endif()

foreach(tutorial_src ${TUTORIAL_SOURCES})
    get_filename_component(tutorial_name ${tutorial_src} NAME_WE)

    add_executable(${tutorial_name} ${tutorial_src})

    target_link_libraries(${tutorial_name} PRIVATE
        libp2p_module_module_plugin
        nlohmann_json::nlohmann_json
    )

    target_include_directories(${tutorial_name} PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/lib
        ${LIBP2P_INCLUDE_DIR}
        ${LOGOS_CPP_SDK_INCLUDE_DIR}
    )

    set_target_properties(${tutorial_name} PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/modules"
        INSTALL_RPATH "$ORIGIN/../lib"
    )

    install(TARGETS ${tutorial_name}
            RUNTIME DESTINATION tutorial)
endforeach()
