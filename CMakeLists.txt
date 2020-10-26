project( BarbacoaServerLib )
cmake_minimum_required( VERSION 3.0.2 )

set(CMAKE_CXX_STANDARD 14)

SET(BOOST_VERSION_MIN "1.58" CACHE STRING "Minimum required Boost Lib version")

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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/emergency_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/event_loop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mt_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/options_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/log_files_watchdog.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/signal_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/fs_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/thread_local_storage.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/app_unit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/app_units_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/integer_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/string_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/msg_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/tcp_client_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/tcp_connection_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/app_connection_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/tcp_server_impl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/network_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/network_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/persist_network_client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/raw_builder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/network/dstream_builder.cpp"
)

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
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mt_server_empty_spec.cpp")
    endif ()
endif ()

list( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/GitVersionGen" )
include( GetGitRevisionDescription )
get_git_head_revision(GIT_REFSPEC SERVER_LIB_GIT_REVISION_SHA)
get_git_unix_timestamp(SERVER_LIB_GIT_REVISION_UNIX_TIMESTAMP)

file(GLOB_RECURSE SERVER_LIB_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/server_lib/*.h")

add_library( server_lib
             ${SERVER_LIB_SOURCES}
             ${SERVER_LIB_HEADERS} )
add_dependencies( server_lib
                  server_clib
                  server_lib_tacopie
                  ssl-helpers
                  moonlight-web)

set(SERVER_LIB_USE_LIBS)
list(APPEND SERVER_LIB_USE_LIBS
    server_clib
    server_lib_tacopie
    ssl-helpers
    moonlight-web
    Threads::Threads
    ${Boost_LIBRARIES}
    ${PLATFORM_SPECIFIC_LIBS})

option(SERVER_LIB_CUSTOM_STACKTRACE_IMPL "Custom stracktrace (ON OR OFF)" ON)

if (NOT SERVER_LIB_CUSTOM_STACKTRACE_IMPL)
    # Try cross platform tool for stack trace
    if ( ${Boost_MINOR_VERSION} GREATER 64 )
        MESSAGE( STATUS "= Boost.Stacktrace 1.0 is used" )

        list(APPEND SERVER_LIB_USE_LIBS
            dl)

        target_compile_definitions( server_lib PUBLIC -DBOOST_STACKTRACE_ADDR2LINE_LOCATION)
    else()
        MESSAGE( FATAL_ERROR  "= Boost version from 1.65 is required for Boost.Stacktrace!" )
    endif() # boost version check
else() # !SERVER_LIB_CUSTOM_STACKTRACE_IMPL
    if (UNIX)
        target_compile_definitions( server_lib PRIVATE -DCUSTOM_STACKTRACE_IMPL)
    else()
        MESSAGE( STATUS "= Stacktrace feature is disabled for current platform" )

        target_compile_definitions( server_lib PRIVATE -DSTACKTRACE_DISABLED)
    endif()
endif() # SERVER_LIB_CUSTOM_STACKTRACE_IMPL

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
option(SERVER_LIB_SUPPRESS_LOGS "Suppress library logs (ON OR OFF)" OFF)

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
