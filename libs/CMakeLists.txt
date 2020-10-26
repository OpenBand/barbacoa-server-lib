set(SERVER_LIB_UNIX_CLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/server-clib" CACHE STRING "Path for Server CLib library in subdirectory")

if (EXISTS "${SERVER_LIB_UNIX_CLIB_PATH}/CMakeLists.txt")
    add_subdirectory(${SERVER_LIB_UNIX_CLIB_PATH})
else()
    add_subdirectory(server-clib)
endif()

add_subdirectory(tacopie)
add_subdirectory(web)
add_subdirectory(ssl-helpers)
