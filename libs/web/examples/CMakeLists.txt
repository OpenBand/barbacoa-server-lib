
add_executable(http_examples http_examples.cpp)
    target_link_libraries(http_examples moonlight-web)
    find_package(Boost 1.53.0 COMPONENTS system thread filesystem REQUIRED)
    target_link_libraries(http_examples ${Boost_LIBRARIES})
    target_include_directories(http_examples PRIVATE ${Boost_INCLUDE_DIR})
    
if(OPENSSL_FOUND)
    add_executable(https_examples https_examples.cpp)
    target_link_libraries(https_examples moonlight-web)
    target_link_libraries(https_examples ${Boost_LIBRARIES})
    target_include_directories(https_examples PRIVATE ${Boost_INCLUDE_DIR})
endif()
