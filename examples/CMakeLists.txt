set(EXAMPLE_ "ex_")

add_executable( sample
                "${CMAKE_CURRENT_SOURCE_DIR}/sample.cpp"
                "${CMAKE_CURRENT_SOURCE_DIR}/err_emulator.cpp" )
set_target_properties(sample PROPERTIES OUTPUT_NAME "${EXAMPLE_}sample")

add_dependencies( sample server_lib )
target_link_libraries( sample
                       server_lib
                       ${PLATFORM_SPECIFIC_LIBS})

add_executable( default_callbacks
               "${CMAKE_CURRENT_SOURCE_DIR}/default_callbacks.cpp"
               "${CMAKE_CURRENT_SOURCE_DIR}/err_emulator.cpp" )
set_target_properties(default_callbacks PROPERTIES OUTPUT_NAME "${EXAMPLE_}default_callbacks")

add_dependencies( default_callbacks server_lib )
target_link_libraries( default_callbacks
                      server_lib
                      ${PLATFORM_SPECIFIC_LIBS})

add_executable( block_pipe
               "${CMAKE_CURRENT_SOURCE_DIR}/block_pipe.cpp" )
set_target_properties(block_pipe PROPERTIES OUTPUT_NAME "${EXAMPLE_}block_pipe")

add_dependencies( block_pipe server_lib )
target_link_libraries( block_pipe
                      server_lib
                      ${PLATFORM_SPECIFIC_LIBS})

if (UNIX)
   add_executable( crash_dump
                   "${CMAKE_CURRENT_SOURCE_DIR}/crash_dump.cpp"
                   "${CMAKE_CURRENT_SOURCE_DIR}/err_emulator.cpp" )
   set_target_properties(crash_dump PROPERTIES OUTPUT_NAME "${EXAMPLE_}crash_dump")

   add_dependencies( crash_dump server_lib )
   target_link_libraries( crash_dump
                          server_lib
                          ${PLATFORM_SPECIFIC_LIBS})
endif() # UNIX
