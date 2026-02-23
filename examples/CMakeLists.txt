# Ensure Qt usage requirements are available in this directory
find_package(Qt6 REQUIRED COMPONENTS Core)

file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS *.cpp)

foreach(example_src ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${example_src} NAME_WE)
    set(target_name example_${example_name})

    add_executable(${target_name} ${example_src})

    target_link_libraries(${target_name} PRIVATE
        Qt6::Core
        libp2p_module_module_plugin
    )

    target_include_directories(${target_name} PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include
    )

    # Logos SDK includes
    if(LOGOS_CPP_SDK_IS_SOURCE)
        target_include_directories(${target_name} PRIVATE
            ${LOGOS_CPP_SDK_ROOT}/cpp
            ${LOGOS_CPP_SDK_ROOT}/cpp/generated
        )
    else()
        target_include_directories(${target_name} PRIVATE
            ${LOGOS_CPP_SDK_ROOT}/include
            ${LOGOS_CPP_SDK_ROOT}/include/cpp
            ${LOGOS_CPP_SDK_ROOT}/include/core
        )
    endif()

    set_target_properties(${target_name} PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/modules"
        INSTALL_RPATH "$ORIGIN/../lib"
    )

    install(TARGETS ${target_name}
            RUNTIME DESTINATION examples)
endforeach()
