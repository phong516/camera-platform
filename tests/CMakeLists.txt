# Unit tests. No external framework: see TestSupport.hpp.
#
# test_http and test_videopipeline only need headers plus the existing
# video_lib, so they build today.

function(add_camera_test target test_name source library)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE ${library})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()

add_camera_test(test_http web.http web/test_http.cpp web_lib)
add_camera_test(test_videopipeline video.videopipeline video/test_videopipeline.cpp video_lib)
add_camera_test(test_profilehost server.profilehost server/test_profilehost.cpp camera_server_lib)
add_camera_test(test_rtspprofileserver server.rtspprofileserver server/test_rtspprofileserver.cpp camera_server_lib)
add_camera_test(test_profilebin server.profilebin server/test_profilebin.cpp camera_server_lib)
add_camera_test(test_application server.application server/test_application.cpp camera_server_lib)

# --- Uncomment once web/ApiController.cpp (and web/WebServer.cpp) exist: ---
# add_executable(test_apicontroller
#     web/test_apicontroller.cpp
# )
#
# target_include_directories(test_apicontroller PRIVATE
#     ${PROJECT_SOURCE_DIR}
# )
#
# target_link_libraries(test_apicontroller PRIVATE
#     web_lib
# )
#
# add_test(NAME web.apicontroller COMMAND test_apicontroller)
