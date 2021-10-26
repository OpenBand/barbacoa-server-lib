add_executable( simple_tcp_server
                "${CMAKE_CURRENT_SOURCE_DIR}/simple_tcp_server.cpp")
add_dependencies( simple_tcp_server server_lib )
target_link_libraries( simple_tcp_server
                       server_lib
                       ${PLATFORM_SPECIFIC_LIBS})

add_executable( simple_tcp_client
                "${CMAKE_CURRENT_SOURCE_DIR}/simple_tcp_client.cpp")
add_dependencies( simple_tcp_client server_lib )
target_link_libraries( simple_tcp_client
                       server_lib
                       ${PLATFORM_SPECIFIC_LIBS})

add_executable( simple_web_server
                "${CMAKE_CURRENT_SOURCE_DIR}/simple_web_server.cpp")
add_dependencies( simple_web_server server_lib )
target_link_libraries( simple_web_server
                       server_lib
                       ${PLATFORM_SPECIFIC_LIBS})

add_executable( app_multithreaded
                "${CMAKE_CURRENT_SOURCE_DIR}/app_multithreaded.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/err_emulator.cpp" )
add_dependencies( app_multithreaded server_lib )
target_link_libraries( app_multithreaded
                       server_lib
                       ${PLATFORM_SPECIFIC_LIBS})

add_executable( app_default_callbacks
               "${CMAKE_CURRENT_SOURCE_DIR}/app_default_callbacks.cpp"
               "${CMAKE_CURRENT_SOURCE_DIR}/err_emulator.cpp" )
add_dependencies( app_default_callbacks server_lib )
target_link_libraries( app_default_callbacks
                      server_lib
                      ${PLATFORM_SPECIFIC_LIBS})

add_executable( app_block_pipe
               "${CMAKE_CURRENT_SOURCE_DIR}/app_block_pipe.cpp" )
add_dependencies( app_block_pipe server_lib )
target_link_libraries( app_block_pipe
                      server_lib
                      ${PLATFORM_SPECIFIC_LIBS})

if (UNIX)
   add_executable( crash_dump
                   "${CMAKE_CURRENT_SOURCE_DIR}/crash_dump.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/err_emulator.cpp" )
   add_dependencies( crash_dump server_lib )
   target_link_libraries( crash_dump
                          server_lib
                          ${PLATFORM_SPECIFIC_LIBS})
endif() # UNIX
