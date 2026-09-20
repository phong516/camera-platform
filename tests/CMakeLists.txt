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
