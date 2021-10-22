project( BarbacoaServerLib )
cmake_minimum_required( VERSION 3.0.2 )

set(CMAKE_CXX_STANDARD 14)

SET(BOOST_VERSION_MIN "1.65" CACHE STRING "Minimum required Boost Lib version")

set(BOOST_COMPONENTS)
list(APPEND BOOST_COMPONENTS system
                             thread
                             program_options
                             log
                             filesystem
                             date_time
                             iostreams
                             regex
                             unit_test_framework)
set( Boost_USE_STATIC_LIBS ON CACHE STRING "ON or OFF" )
set( Boost_USE_MULTITHREADED ON CACHE STRING "ON or OFF" )

find_package(Boost ${BOOST_VERSION_MIN} REQUIRED COMPONENTS ${BOOST_COMPONENTS})
find_package(Threads REQUIRED)

include_directories( ${Boost_INCLUDE_DIR} )

add_subdirectory( libs )

set(SERVER_LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logger.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/logging_trace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/emergency_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/event_loop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main_loop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mt_event_loop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/application_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/options_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/log_files_watchdog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/signal_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/change_current_dir.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/thread_local_storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/thread_sync_helpers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/unit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/unit_builder_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/integer_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/string_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/msg_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/scope_runner.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/transport/tcp_client_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/transport/asio_connection_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/transport/tcp_client_connection_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/transport/tcp_server_connection_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/transport/tcp_server_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/connection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/client_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/server_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/raw_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/dstream_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/ifconfig.cpp"
)

list(APPEND SERVER_LIB_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/server_lib_revision.cpp")

if (NOT UNIX)
    if (WIN32)
        file(GLOB_RECURSE WIN_SPECIFIC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/win/*.cpp")
        file(GLOB_RECURSE WIN_SPECIFIC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/win/*.h")
        list(APPEND SERVER_LIB_SOURCES
            "${WIN_SPECIFIC_SOURCES}")
        list(APPEND SERVER_LIB_SOURCES
            "${WIN_SPECIFIC_HEADERS}")
    else()
        list(APPEND SERVER_LIB_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/src/app_empty_spec.cpp")
    endif ()
endif ()

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/GitVersionGen" )
include( GetGitRevisionDescription )
get_git_head_revision(GIT_REFSPEC SERVER_LIB_GIT_REVISION_SHA)
get_git_unix_timestamp(SERVER_LIB_GIT_REVISION_UNIX_TIMESTAMP)
git_describe(SERVER_LIB_GIT_REVISION_DESCRIPTION)

file(GLOB_RECURSE SERVER_LIB_IMPL_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE SERVER_LIB_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/server_lib/*.h")

add_library( server_lib
             ${SERVER_LIB_SOURCES}
             ${SERVER_LIB_IMPL_HEADERS}
             ${SERVER_LIB_HEADERS} )
add_dependencies( server_lib
                  server_clib
                  ssl-helpers
                  moonlight-web)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/version SERVER_LIB_VERSION_CONTENT)
list(GET SERVER_LIB_VERSION_CONTENT 0 SERVER_LIB_VERSION)

add_custom_target( server_lib_docs SOURCES
                   ${CMAKE_CURRENT_SOURCE_DIR}/version
                   ${CMAKE_CURRENT_SOURCE_DIR}/README.md
                   ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.ini
                   ${CMAKE_CURRENT_SOURCE_DIR}/src/server_lib_revision.cpp.in)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.ini" "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/server_lib_revision.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/server_lib_revision.cpp" @ONLY)

set(PLATFORM_SPECIFIC_LIBS)

option(SERVER_LIB_DETAILED_STACKTRACE "Add maximum info to crush stacktrace (ON OR OFF)" OFF)

if ( ${Boost_MINOR_VERSION} GREATER 64 )
    if (UNIX)
         MESSAGE( STATUS "Boost.Stacktrace/addr2line is used" )

        target_compile_definitions(server_lib PRIVATE BOOST_STACKTRACE_USE_ADDR2LINE)
        list(APPEND PLATFORM_SPECIFIC_LIBS
            dl
        )
    elseif (MINGW AND BOOST_STACKTRACE_BACKTRACE_LIBRARY)
        MESSAGE( STATUS "Boost.Stacktrace/backtrace is used" )

        target_compile_definitions(server_lib PRIVATE BOOST_STACKTRACE_USE_BACKTRACE)
        list(APPEND PLATFORM_SPECIFIC_LIBS
            boost_stacktrace_backtrace backtrace
        )
    else ()
        MESSAGE( STATUS "Boost.Stacktrace/basic is used" )
    endif ()

    if (SERVER_LIB_DETAILED_STACKTRACE)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        MESSAGE( STATUS "Detailed stacktrace - ON (Experimental)" )

        target_compile_definitions(server_lib PUBLIC _GLIBCXX_ASSERTIONS)
        # On debug info
        target_compile_options(server_lib PUBLIC "-g")
        # Off inline optimization
        target_compile_options(server_lib PUBLIC "-fno-inline")
        # PIE makes the code position independent and could mess crash dump.
        # A result of PIE is that Address Space Layout Randomization (ASLR) can be enabled
        # (these days the default for security reasons, which is why also PIE is the default these days)
        target_link_options(server_lib PUBLIC "-no-pie")
    endif ()
    endif ()
else()
    MESSAGE( FATAL_ERROR  "Boost version from 1.65 is required for Boost.Stacktrace!" )
endif() # boost version check

list(APPEND SERVER_LIB_USE_LIBS
    server_clib
    ssl-helpers
    moonlight-web
    Threads::Threads
    ${Boost_LIBRARIES}
    ${PLATFORM_SPECIFIC_LIBS})

target_link_libraries( server_lib
              ${SERVER_LIB_USE_LIBS})

target_include_directories( server_lib
                         PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

option(SERVER_LIB_USE_SERVER_LOGS "Use extended Linux logs (ON OR OFF)" ON)

if( SERVER_LIB_USE_SERVER_LOGS )
    target_compile_definitions( server_lib PRIVATE -DUSE_SERVER_LOGS)
endif()

option(SERVER_LIB_DUPLICATE_LOGS_TO_COUT "Print logs not only to default loger but to tty output (ON OR OFF)" OFF)

if( SERVER_LIB_DUPLICATE_LOGS_TO_COUT )
    target_compile_definitions( server_lib PRIVATE -DDUPLICATE_LOGS_TO_COUT)
endif()

option(SERVER_LIB_TEST_PLATFORM_ANDROID "Forcibly set PLATFORM = ANDROID to test compiling (ON OR OFF)" OFF)
option(SERVER_LIB_TEST_PLATFORM_IOS "Forcibly set PLATFORM = IOS to test compiling (ON OR OFF)" OFF)
option(SERVER_LIB_TEST_PLATFORM_WINDOWS "Forcibly set PLATFORM = WINDOWS to test compiling (ON OR OFF)" OFF)

if (SERVER_LIB_TEST_PLATFORM_ANDROID)
    target_compile_definitions( server_lib PRIVATE -DTEST_PLATFORM_ANDROID)
else()
if (SERVER_LIB_TEST_PLATFORM_IOS)
    target_compile_definitions( server_lib PRIVATE -DTEST_PLATFORM_IOS)
else()
   if (SERVER_LIB_TEST_PLATFORM_WINDOWS)
       target_compile_definitions( server_lib PRIVATE -DTEST_PLATFORM_WINDOWS)
   endif()
endif()
endif()

option(SERVER_LIB_SUPPRESS_ASSERT_DIALOG "Suppress Windows platform specific Assert dialog (ON OR OFF)" ON)
option(SERVER_LIB_SUPPRESS_LOGS "Suppress library logs (ON OR OFF)" ON)

if (SERVER_LIB_SUPPRESS_ASSERT_DIALOG)
    target_compile_definitions( server_lib PUBLIC -DSUPPRESS_ASSERT_DIALOG)
endif()

if (SERVER_LIB_SUPPRESS_LOGS)
    target_compile_definitions( server_lib PUBLIC -DSERVER_LIB_SUPPRESS_LOGS)
endif()

target_compile_definitions(server_lib PUBLIC -DSERVER_LIB_GIT_REVISION_SHA="${SERVER_LIB_GIT_REVISION_SHA}"
                                             -DSERVER_LIB_GIT_REVISION_UNIX_TIMESTAMP="${SERVER_LIB_GIT_REVISION_UNIX_TIMESTAMP}")

option ( SERVER_LIB_BUILD_TESTS "Build tests (ON OR OFF). This option makes sense only for integrated library!" OFF)
option ( SERVER_LIB_BUILD_EXAMPLES "Build examples (ON OR OFF). This option makes sense only for integrated library!" OFF)

# If this lib is not a sub-project:
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    set(SERVER_LIB_BUILD_TESTS ON)
    set(SERVER_LIB_BUILD_EXAMPLES ON)
endif()

if ( SERVER_LIB_BUILD_TESTS )

    file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    file(GLOB_RECURSE HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")

    add_executable(server_lib_tests ${SOURCES} ${HEADERS})

    add_dependencies( server_lib_tests
                   server_lib)
    target_include_directories( server_lib_tests
                        PRIVATE "${Boost_INCLUDE_DIR}")
    target_link_libraries( server_lib_tests
                        server_lib
                        ${Boost_LIBRARIES}
                        ${PLATFORM_SPECIFIC_LIBS})

endif()

if ( SERVER_LIB_BUILD_EXAMPLES )
    add_subdirectory(examples)
endif()
