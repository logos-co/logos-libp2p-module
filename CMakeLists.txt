cmake_minimum_required(VERSION 3.14)
project(Libp2pModulePlugin LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_AUTOMOC ON)

option(LOGOS_LIBP2P_MODULE_USE_VENDOR "Force use of vendored Logos dependencies" OFF)

# Allow override from environment or command line
if(NOT DEFINED LOGOS_LIBLOGOS_ROOT)
    set(_parent_liblogos "${CMAKE_CURRENT_SOURCE_DIR}/../liblogos")
    set(_use_vendor ${LOGOS_LIBP2P_MODULE_USE_VENDOR})
    if(NOT _use_vendor)
        if(NOT EXISTS "${_parent_liblogos}/interface.h")
            set(_use_vendor ON)
        endif()
    endif()
    if(_use_vendor)
        set(LOGOS_LIBLOGOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/liblogos")
    else()
        set(LOGOS_LIBLOGOS_ROOT "${_parent_liblogos}")
    endif()
endif()

if(NOT DEFINED LOGOS_CPP_SDK_ROOT)
    set(_parent_cpp_sdk "${CMAKE_CURRENT_SOURCE_DIR}/../logos-cpp-sdk")
    set(_use_vendor ${LOGOS_LIBP2P_MODULE_USE_VENDOR})
    if(NOT _use_vendor)
        if(NOT EXISTS "${_parent_cpp_sdk}/cpp/logos_api.h")
            set(_use_vendor ON)
        endif()
    endif()
    if(_use_vendor)
        set(LOGOS_CPP_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/logos-cpp-sdk")
    else()
        set(LOGOS_CPP_SDK_ROOT "${_parent_cpp_sdk}")
    endif()
endif()

# Check if dependencies are available (support both source and installed layouts)

# LIBLOGOS
set(_liblogos_found FALSE)
if(EXISTS "${LOGOS_LIBLOGOS_ROOT}/interface.h")
    set(_liblogos_found TRUE)
    set(_liblogos_is_source TRUE)
elseif(EXISTS "${LOGOS_LIBLOGOS_ROOT}/include/interface.h")
    set(_liblogos_found TRUE)
    set(_liblogos_is_source FALSE)
endif()
if(NOT _liblogos_found)
    message(FATAL_ERROR "liblogos not found at ${LOGOS_LIBLOGOS_ROOT}. "
                        "Set LOGOS_LIBLOGOS_ROOT or run git submodule update --init --recursive.")
endif()

# CPP SDK
set(_cpp_sdk_found FALSE)
if(EXISTS "${LOGOS_CPP_SDK_ROOT}/cpp/logos_api.h")
    set(_cpp_sdk_found TRUE)
    set(_cpp_sdk_is_source TRUE)
elseif(EXISTS "${LOGOS_CPP_SDK_ROOT}/include/cpp/logos_api.h")
    set(_cpp_sdk_found TRUE)
    set(_cpp_sdk_is_source FALSE)
endif()
if(NOT _cpp_sdk_found)
    message(FATAL_ERROR "logos-cpp-sdk not found at ${LOGOS_CPP_SDK_ROOT}. "
                        "Set LOGOS_CPP_SDK_ROOT or run git submodule update --init --recursive.")
endif()


# LIBP2P
set(_libp2p_found FALSE)
if(EXISTS "${LIBP2P_ROOT}/libp2p.nimble")
    set(_libp2p_found TRUE)
    set(_libp2p_is_source TRUE)
elseif(EXISTS "${LOGOS_CPP_SDK_ROOT}/include/cpp/logos_api.h")
    set(_libp2p_found TRUE)
    set(_libp2p_is_source FALSE)
endif()
if(NOT _libp2p_found)
    message(FATAL_ERROR "nim-libp2p not found at ${LIBP2P_ROOT}. "
        "Set LIBP2P_ROOT or run git submodule update --init --recursive.")
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

# Try to find the generator binary
if(_cpp_sdk_is_source)
    # Source layout: build the generator
    set(CPP_GENERATOR_BUILD_DIR "${LOGOS_DEPS_ROOT}/build/cpp-generator")
    set(CPP_GENERATOR "${CPP_GENERATOR_BUILD_DIR}/bin/logos-cpp-generator")

    if(NOT TARGET cpp_generator_build)
        add_custom_target(cpp_generator_build
            COMMAND bash "${LOGOS_CPP_SDK_ROOT}/cpp-generator/compile.sh"
            WORKING_DIRECTORY "${LOGOS_DEPS_ROOT}"
            COMMENT "Building logos-cpp-generator via ${LOGOS_CPP_SDK_ROOT}/cpp-generator/compile.sh"
            VERBATIM
        )
    endif()

    add_custom_target(run_cpp_generator_libp2p
        COMMAND "${CPP_GENERATOR}" --metadata "${METADATA_JSON}" --module-dir "${PLUGINS_OUTPUT_DIR}"
        WORKING_DIRECTORY "${LOGOS_DEPS_ROOT}"
        COMMENT "Running logos-cpp-generator on ${METADATA_JSON} with module-dir ${PLUGINS_OUTPUT_DIR}"
        VERBATIM
    )
    add_dependencies(run_cpp_generator_libp2p cpp_generator_build)
else()
    # Installed layout: use the generator from the install path
    find_program(CPP_GENERATOR logos-cpp-generator PATHS ${LOGOS_CPP_SDK_ROOT}/bin NO_DEFAULT_PATH REQUIRED)

    add_custom_target(run_cpp_generator_libp2p
        COMMAND "${CPP_GENERATOR}" --metadata "${METADATA_JSON}" --module-dir "${PLUGINS_OUTPUT_DIR}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Running logos-cpp-generator on ${METADATA_JSON} with module-dir ${PLUGINS_OUTPUT_DIR}"
        VERBATIM
    )
endif()

set(LIBP2P_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
# Prioritize platform-specific library names
if(APPLE)
    set(LIBP2P_NAMES libp2p.dylib libp2p.so libp2p.dll)
elseif(WIN32)
    set(LIBP2P_NAMES libp2p.dll libp2p.dylib libp2p.so)
else()
    set(LIBP2P_NAMES libp2p.so libp2p.dylib libp2p.dll)
endif()
# Always add lib directory and try to link -llibp2p; also keep optional direct path if present
find_library(LIBP2P_PATH NAMES ${LIBP2P_NAMES} PATHS ${LIBP2P_DIR} NO_DEFAULT_PATH)

# Plugin sources
set(PLUGIN_SOURCES
    libp2p_module_plugin.cpp
    libp2p_module_plugin.h
    libp2p_module_interface.h
)

# Add liblogos interface header
if(_liblogos_is_source)
    list(APPEND PLUGIN_SOURCES ${LOGOS_LIBLOGOS_ROOT}/interface.h)
else()
    list(APPEND PLUGIN_SOURCES ${LOGOS_LIBLOGOS_ROOT}/include/interface.h)
endif()

# Add SDK sources (only if source layout, installed layout uses the library)
if(_cpp_sdk_is_source)
    list(APPEND PLUGIN_SOURCES
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api.cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api.h
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api_client.cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api_client.h
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api_consumer.cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api_consumer.h
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api_provider.cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/logos_api_provider.h
        ${LOGOS_CPP_SDK_ROOT}/cpp/token_manager.cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/token_manager.h
        ${LOGOS_CPP_SDK_ROOT}/cpp/module_proxy.cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/module_proxy.h
    )
endif()

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

# Link SDK library if using installed layout
if(NOT _cpp_sdk_is_source)
    find_library(LOGOS_SDK_LIB logos_sdk PATHS ${LOGOS_CPP_SDK_ROOT}/lib NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(libp2p_module_plugin PRIVATE ${LOGOS_SDK_LIB})
endif()

# Link to libp2p on all platforms
if(LIBP2P_PATH)
    target_link_libraries(libp2p_module_plugin PRIVATE ${LIBP2P_PATH})
else()
    message(WARNING "libp2p not found in lib/. Build or provide it before linking.")
endif()

# Include directories
target_include_directories(libp2p_module_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBP2P_DIR}
)

# Add include directories based on layout type
if(_liblogos_is_source)
    target_include_directories(libp2p_module_plugin PRIVATE ${LOGOS_LIBLOGOS_ROOT})
else()
    target_include_directories(libp2p_module_plugin PRIVATE ${LOGOS_LIBLOGOS_ROOT}/include)
endif()

if(_cpp_sdk_is_source)
    target_include_directories(libp2p_module_plugin PRIVATE 
        ${LOGOS_CPP_SDK_ROOT}/cpp
        ${LOGOS_CPP_SDK_ROOT}/cpp/generated
    )
else()
    target_include_directories(libp2p_module_plugin PRIVATE 
        ${LOGOS_CPP_SDK_ROOT}/include
        ${LOGOS_CPP_SDK_ROOT}/include/cpp
        ${LOGOS_CPP_SDK_ROOT}/include/core
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

    if(LIBP2P_PATH)
        get_filename_component(LIBP2P_FILENAME "${LIBP2P_PATH}" NAME)
        add_custom_command(TARGET libp2p_module_plugin PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${LIBP2P_PATH}
                ${CMAKE_BINARY_DIR}/modules/${LIBP2P_FILENAME}
            COMMENT "Copying ${LIBP2P_FILENAME} to modules directory"
        )

        add_custom_command(TARGET libp2p_module_plugin POST_BUILD
            COMMAND install_name_tool -id "@rpath/libp2p_module_plugin.dylib"
            $<TARGET_FILE:libp2p_module_plugin>
            COMMAND install_name_tool -id "@rpath/${LIBP2P_FILENAME}" "${CMAKE_BINARY_DIR}/modules/${LIBP2P_FILENAME}"
            COMMAND install_name_tool -change "${LIBP2P_PATH}" "@rpath/${LIBP2P_FILENAME}" $<TARGET_FILE:libp2p_module_plugin>
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

    if(LIBP2P_PATH)
        get_filename_component(LIBP2P_FILENAME "${LIBP2P_PATH}" NAME)
        add_custom_command(TARGET libp2p_module_plugin PRE_LINK
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${LIBP2P_PATH}
                ${CMAKE_BINARY_DIR}/modules/${LIBP2P_FILENAME}
            COMMENT "Copying ${LIBP2P_FILENAME} to modules directory"
        )
    endif()
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
