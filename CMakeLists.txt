cmake_minimum_required(VERSION 3.16)

project(
    xlib
    VERSION 2.1.0
    DESCRIPTION "A lightweight cross-platform low-level systems library for C and C++"
    LANGUAGES C CXX
)

option(XLIB_BUILD_EXAMPLES "Build xlib example programs" ON)
option(XLIB_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer where supported" OFF)
option(XLIB_ENABLE_TRACING "Enable tracing hooks in public API functions" OFF)

add_library(xlib
    src/x_allocator.c
    src/x_diagnostics.c
    src/x_dynamic_library.c
    src/x_event.c
    src/x_filesystem.c
    src/x_ipc.c
    src/x_memory.c
    src/x_network.c
    src/x_process.c
    src/x_terminal.c
    src/x_thread.c
    src/x_time.c
    src/x_tls.c
)

add_library(xlib::xlib ALIAS xlib)

get_target_property(target_type xlib TYPE)
if (target_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(xlib PUBLIC XLIB_STATIC)
else()
    target_compile_definitions(xlib PRIVATE XLIB_BUILDING)
    set_target_properties(xlib PROPERTIES C_VISIBILITY_PRESET hidden)
    set_target_properties(xlib PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(xlib PROPERTIES VISIBILITY_INLINES_HIDDEN YES)
endif()
target_compile_features(xlib PUBLIC c_std_99)

target_include_directories(xlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_compile_definitions(xlib PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(xlib PUBLIC ws2_32 iphlpapi)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(xlib PUBLIC Threads::Threads)
    if(CMAKE_DL_LIBS)
        target_link_libraries(xlib PUBLIC ${CMAKE_DL_LIBS})
    endif()
    # shm_open and sem_open require librt on Linux (glibc < 2.17)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(xlib PUBLIC rt)
    endif()
endif()

if(MSVC)
    target_compile_options(xlib PRIVATE /W4)
else()
    target_compile_options(xlib PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(XLIB_ENABLE_SANITIZERS)
    if(MSVC OR WIN32)
        message(WARNING "XLIB_ENABLE_SANITIZERS is currently supported only for non-Windows GCC/Clang builds")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(xlib PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(xlib PUBLIC -fsanitize=address,undefined)
    else()
        message(WARNING "XLIB_ENABLE_SANITIZERS requires GCC or Clang")
    endif()
endif()

if(XLIB_ENABLE_TRACING)
    target_compile_definitions(xlib PUBLIC XLIB_ENABLE_TRACING)
endif()

if(XLIB_BUILD_EXAMPLES)
    enable_testing()

    add_executable(xlib_thread_example examples/thread_example.c)
    target_link_libraries(xlib_thread_example PRIVATE xlib::xlib)

    add_test(NAME xlib_thread_example COMMAND xlib_thread_example)

    add_executable(xlib_threading_primitives_example examples/threading_primitives_example.c)
    target_link_libraries(xlib_threading_primitives_example PRIVATE xlib::xlib)

    add_test(NAME xlib_threading_primitives_example COMMAND xlib_threading_primitives_example)

    add_executable(xlib_time_example examples/time_example.c)
    target_link_libraries(xlib_time_example PRIVATE xlib::xlib)

    add_test(NAME xlib_time_example COMMAND xlib_time_example)

    add_executable(xlib_memory_example examples/memory_example.c)
    target_link_libraries(xlib_memory_example PRIVATE xlib::xlib)

    add_test(NAME xlib_memory_example COMMAND xlib_memory_example)

    add_executable(xlib_filesystem_example examples/filesystem_example.c)
    target_link_libraries(xlib_filesystem_example PRIVATE xlib::xlib)

    add_test(NAME xlib_filesystem_example COMMAND xlib_filesystem_example)

    add_library(xlib_dynamic_library_plugin SHARED examples/dynamic_library_plugin.c)
    target_compile_definitions(xlib_dynamic_library_plugin PRIVATE XLIB_DYNAMIC_PLUGIN_BUILD)

    add_executable(xlib_dynamic_library_example examples/dynamic_library_example.c)
    target_link_libraries(xlib_dynamic_library_example PRIVATE xlib::xlib)

    add_test(
        NAME xlib_dynamic_library_example
        COMMAND xlib_dynamic_library_example $<TARGET_FILE:xlib_dynamic_library_plugin>
    )

    add_executable(xlib_network_example examples/network_example.c)
    target_link_libraries(xlib_network_example PRIVATE xlib::xlib)

    add_test(NAME xlib_network_example COMMAND xlib_network_example)

    add_executable(xlib_tcp_server_example examples/tcp_server_example.c)
    target_link_libraries(xlib_tcp_server_example PRIVATE xlib::xlib)

    add_executable(xlib_tcp_client_example examples/tcp_client_example.c)
    target_link_libraries(xlib_tcp_client_example PRIVATE xlib::xlib)

    add_executable(xlib_process_helper examples/process_helper.c)
    target_link_libraries(xlib_process_helper PRIVATE xlib::xlib)

    add_executable(xlib_process_example examples/process_example.c)
    target_link_libraries(xlib_process_example PRIVATE xlib::xlib)

    add_test(
        NAME xlib_process_example
        COMMAND xlib_process_example $<TARGET_FILE:xlib_process_helper>
    )

    add_executable(xlib_event_example examples/event_example.c)
    target_link_libraries(xlib_event_example PRIVATE xlib::xlib)

    add_test(NAME xlib_event_example COMMAND xlib_event_example)

    add_executable(xlib_filesystem_process_extensions_example examples/filesystem_process_extensions_example.c)
    target_link_libraries(xlib_filesystem_process_extensions_example PRIVATE xlib::xlib)

    add_test(
        NAME xlib_filesystem_process_extensions_example
        COMMAND xlib_filesystem_process_extensions_example $<TARGET_FILE:xlib_process_helper>
    )

    add_executable(xlib_ipc_example examples/ipc_example.c)
    target_link_libraries(xlib_ipc_example PRIVATE xlib::xlib)

    add_test(NAME xlib_ipc_example COMMAND xlib_ipc_example)

    add_executable(xlib_networking_extensions_example examples/networking_extensions_example.c)
    target_link_libraries(xlib_networking_extensions_example PRIVATE xlib::xlib)

    add_test(NAME xlib_networking_extensions_example COMMAND xlib_networking_extensions_example)

    add_executable(xlib_diagnostics_example examples/diagnostics_example.c)
    target_link_libraries(xlib_diagnostics_example PRIVATE xlib::xlib)

    add_test(NAME xlib_diagnostics_example COMMAND xlib_diagnostics_example)

    add_executable(xlib_tls_example examples/tls_example.c)
    target_link_libraries(xlib_tls_example PRIVATE xlib::xlib)

    add_test(NAME xlib_tls_example COMMAND xlib_tls_example)

    add_executable(xlib_stress_test examples/stress_test.c)
    target_link_libraries(xlib_stress_test PRIVATE xlib::xlib)
    
    add_test(NAME xlib_stress_test COMMAND xlib_stress_test)

    add_executable(xlib_cpp_wrappers_v17_example examples/cpp_wrappers_v17_example.cpp)
    target_compile_features(xlib_cpp_wrappers_v17_example PRIVATE cxx_std_17)
    target_link_libraries(xlib_cpp_wrappers_v17_example PRIVATE xlib::xlib)

    add_test(
        NAME xlib_cpp_wrappers_v17_example
        COMMAND xlib_cpp_wrappers_v17_example $<TARGET_FILE:xlib_process_helper>
    )

    add_executable(xlib_cpp_wrapper_example examples/cpp_wrapper_example.cpp)
    target_compile_features(xlib_cpp_wrapper_example PRIVATE cxx_std_11)
    target_link_libraries(xlib_cpp_wrapper_example PRIVATE xlib::xlib)

    add_test(
        NAME xlib_cpp_wrapper_example
        COMMAND xlib_cpp_wrapper_example $<TARGET_FILE:xlib_dynamic_library_plugin>
    )
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/xlibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xlibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/xlibConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xlib
)

install(
    TARGETS xlib
    EXPORT xlibTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    FILES
        readme.md
        LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(
    DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
)

install(
    EXPORT xlibTargets
    NAMESPACE xlib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xlib
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/xlibConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/xlibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xlib
)
