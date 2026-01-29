cmake_minimum_required(VERSION 3.14)
project(Libp2pModulePlugin LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)

option(FORCE_VENDOR "Force use of vendored Logos dependencies" OFF)

if(NOT DEFINED LIBP2P_ROOT)
    message(FATAL_ERROR "LIBP2P_ROOT not defined."
        "Set LIBP2P_ROOT")
endif()

if(NOT DEFINED LOGOS_LIBLOGOS_ROOT)
    message(FATAL_ERROR "LOGOS_LIBLOGOS_ROOT not defined."
        "Set LOGOS_LIBLOGOS_ROOT")
endif()

if(NOT DEFINED LOGOS_CPP_SDK_ROOT)
    message(FATAL_ERROR "LOGOS_CPP_SDK_ROOT not defined."
        "Set LOGOS_CPP_SDK_ROOT")
endif()


# Root that contains the vendored dependencies (logos-core checkout or script vendor directory)
get_filename_component(LOGOS_DEPS_ROOT "${LOGOS_CPP_SDK_ROOT}" DIRECTORY)

# Find Qt RemoteObjects (needed for LogosAPI)
if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core RemoteObjects)
    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
    else()
        set(QT_VERSION_MAJOR 5)
    endif()
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core RemoteObjects)

# Run Logos C++ generator on metadata before compilation
set(METADATA_JSON "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json")
set(PLUGINS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")

# Installed layout: use the generator from the install path
find_program(CPP_GENERATOR logos-cpp-generator PATHS ${LOGOS_CPP_SDK_ROOT}/bin NO_DEFAULT_PATH REQUIRED)

add_custom_target(run_cpp_generator_libp2p
    COMMAND "${CPP_GENERATOR}" --metadata "${METADATA_JSON}" --module-dir "${PLUGINS_OUTPUT_DIR}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running logos-cpp-generator on ${METADATA_JSON} with module-dir ${PLUGINS_OUTPUT_DIR}"
    VERBATIM
)

# Plugin sources
set(PLUGIN_SOURCES
    libp2p_module_plugin.cpp
    libp2p_module_plugin.h
    libp2p_module_interface.h
)

# Add liblogos interface header
list(APPEND PLUGIN_SOURCES ${LOGOS_LIBLOGOS_ROOT}/include/interface.h)

# Create the plugin library
add_library(libp2p_module_plugin SHARED ${PLUGIN_SOURCES})

# Set output name without lib prefix
set_target_properties(libp2p_module_plugin PROPERTIES
    PREFIX "")

# Ensure generator runs before building the plugin
add_dependencies(libp2p_module_plugin run_cpp_generator_libp2p)

# Link Qt libraries
target_link_libraries(libp2p_module_plugin PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::RemoteObjects
)


# Link SDK libraries
target_include_directories(libp2p_module_plugin PRIVATE
    ${LOGOS_CPP_SDK_ROOT}/include
    ${LOGOS_CPP_SDK_ROOT}/include/cpp
    ${LOGOS_CPP_SDK_ROOT}/include/core
)
find_library(LOGOS_SDK_LIB logos_sdk PATHS ${LOGOS_CPP_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
target_link_libraries(libp2p_module_plugin PRIVATE ${LOGOS_SDK_LIB})

# Link Libp2p libraries
target_include_directories(libp2p_module_plugin PRIVATE
    ${LIBP2P_ROOT}/include
)

if(APPLE)
    target_link_libraries(libp2p_module_plugin PRIVATE
        ${LIBP2P_ROOT}/lib/libp2p.dylib
    )
elseif(WIN32)
    target_link_libraries(libp2p_module_plugin PRIVATE
        ${LIBP2P_ROOT}/lib/libp2p.dll
    )
else()
    target_link_libraries(libp2p_module_plugin PRIVATE
        ${LIBP2P_ROOT}/lib/libp2p.so
    )
endif()

set_target_properties(libp2p_module_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules"  # For Windows .dll
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE)

if(APPLE)
    # Allow unresolved symbols at link time; libp2p will be provided at runtime
    target_link_options(libp2p_module_plugin PRIVATE -undefined dynamic_lookup)
    set_target_properties(libp2p_module_plugin PROPERTIES
        INSTALL_RPATH "@loader_path"
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR TRUE)

    if(LIBP2P_ROOT)
        set(LIBP2P_LIBRARY_PATH "${LIBP2P_ROOT}/lib/libp2p.dylib")
        add_custom_command(TARGET libp2p_module_plugin POST_BUILD
            COMMAND install_name_tool -id "@rpath/libp2p_module_plugin.dylib"
                $<TARGET_FILE:libp2p_module_plugin>
            COMMAND install_name_tool -change "${LIBP2P_LIBRARY_PATH}" "@rpath/libp2p.dylib"
                $<TARGET_FILE:libp2p_module_plugin>
            COMMENT "Updating library paths for macOS"
        )
    else()
        add_custom_command(TARGET libp2p_module_plugin POST_BUILD
            COMMAND install_name_tool -id "@rpath/libp2p_module_plugin.dylib"
            $<TARGET_FILE:libp2p_module_plugin>
            COMMENT "Updating library paths for macOS (libp2p not found)"
        )
    endif()
else()
    set_target_properties(libp2p_module_plugin PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        INSTALL_RPATH_USE_LINK_PATH FALSE)
endif()

install(TARGETS libp2p_module_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/logos/modules
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/logos/modules
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/logos/modules
)

install(FILES ${METADATA_JSON}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/logos-libp2p-module
)

install(DIRECTORY "${PLUGINS_OUTPUT_DIR}/"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/logos-libp2p-module/generated
    OPTIONAL
)
