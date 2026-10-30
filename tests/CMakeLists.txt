# Unit tests. No external framework: see TestSupport.hpp.
#
# test_http and test_videopipeline only need headers plus the existing
# video_lib, so they build today.

add_executable(test_http
    web/test_http.cpp
)

target_include_directories(test_http PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(test_http PRIVATE
    web_lib
)

add_test(NAME web.http COMMAND test_http)

add_executable(test_videopipeline
    video/test_videopipeline.cpp
)

target_include_directories(test_videopipeline PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(test_videopipeline PRIVATE
    video_lib
)

add_test(NAME video.videopipeline COMMAND test_videopipeline)

add_executable(test_profilehost
    server/test_profilehost.cpp
)

target_include_directories(test_profilehost PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(test_profilehost PRIVATE
    camera_server_lib
)

add_test(NAME server.profilehost COMMAND test_profilehost)

add_executable(test_rtspprofileserver
    server/test_rtspprofileserver.cpp
)

target_include_directories(test_rtspprofileserver PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(test_rtspprofileserver PRIVATE
    camera_server_lib
)

add_test(NAME server.rtspprofileserver COMMAND test_rtspprofileserver)

add_executable(test_profilebin
    server/test_profilebin.cpp
)

target_include_directories(test_profilebin PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(test_profilebin PRIVATE
    camera_server_lib
)

add_test(NAME server.profilebin COMMAND test_profilebin)

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
