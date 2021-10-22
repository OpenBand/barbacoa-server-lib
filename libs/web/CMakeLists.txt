cmake_minimum_required (VERSION 3.0)

## HTTP(S) Server/Client
# (based on boost::asio)
#
project (moonlight-web)

option(WEB_USE_STANDALONE_ASIO "set ON to use standalone Asio instead of Boost.Asio" OFF)

set(CMAKE_CXX_STANDARD 14)

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wsign-conversion)
else()
    add_compile_options(/W1)
endif()

set( SOLUTION_FILES
    include/moonlight/web/server_http.hpp
    include/moonlight/web/client_http.hpp
    include/moonlight/web/server_https.hpp
    include/moonlight/web/client_https.hpp
    include/moonlight/web/utility.hpp
    include/moonlight/web/status_code.hpp)

add_custom_target( moonlight-web-src SOURCES ${SOLUTION_FILES})
add_library(moonlight-web INTERFACE)

target_include_directories(moonlight-web INTERFACE
                           ${CMAKE_CURRENT_SOURCE_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(moonlight-web INTERFACE ${CMAKE_THREAD_LIBS_INIT})

# TODO 2020 when Debian Jessie LTS ends:
# Remove Boost system, thread, regex components; use Boost::<component> aliases; remove Boost target_include_directories
if(WEB_USE_STANDALONE_ASIO)
    target_compile_definitions(moonlight-web INTERFACE WEB_USE_STANDALONE_ASIO)
    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(asio.hpp HAVE_ASIO)
    if(NOT HAVE_ASIO)
        message(FATAL_ERROR "Standalone Asio not found")
    endif()
else()
    find_package(Boost 1.53.0 COMPONENTS system thread REQUIRED)
    target_link_libraries(moonlight-web INTERFACE ${Boost_LIBRARIES})
    target_include_directories(moonlight-web INTERFACE ${Boost_INCLUDE_DIR})
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        target_compile_definitions(moonlight-web INTERFACE USE_BOOST_REGEX)
        find_package(Boost 1.53.0 COMPONENTS regex REQUIRED)
        target_link_libraries(moonlight-web INTERFACE ${Boost_LIBRARIES})
        target_include_directories(moonlight-web INTERFACE ${Boost_INCLUDE_DIR})
    endif()
endif()
if(WIN32)
    target_link_libraries(moonlight-web INTERFACE ws2_32 wsock32)
endif()

if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()
find_package(OpenSSL)
if(OPENSSL_FOUND)
    target_compile_definitions(moonlight-web INTERFACE HAVE_OPENSSL)
    target_link_libraries(moonlight-web INTERFACE ${OPENSSL_LIBRARIES})
    target_include_directories(moonlight-web INTERFACE ${OPENSSL_INCLUDE_DIR})
endif()

option(WEB_BUILD_TESTS "Build Web tests (ON OR OFF). This option makes sense only for integrated library!" OFF)
option(WEB_BUILD_EXAMPLES "Build Web examples (ON OR OFF). This option makes sense only for integrated library!" OFF)

# If Simple-Web-Server is not a sub-project:
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    add_subdirectory(examples)
    
    set(WEB_BUILD_TESTS ON)
endif()

if(WEB_BUILD_TESTS)
    message( STATUS "Configuring ctest-s" )
    enable_testing()
    add_subdirectory(tests)
endif()
