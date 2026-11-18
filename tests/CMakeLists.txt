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
add_camera_test(test_webserver web.webserver web/test_webserver.cpp web_lib)
add_camera_test(test_videopipeline video.videopipeline video/test_videopipeline.cpp video_lib)
add_camera_test(test_profilehost server.profilehost server/test_profilehost.cpp camera_server_lib)
add_camera_test(test_rtspprofileserver server.rtspprofileserver server/test_rtspprofileserver.cpp camera_server_lib)
add_camera_test(test_profilebin server.profilebin server/test_profilebin.cpp camera_server_lib)
add_camera_test(test_application server.application server/test_application.cpp camera_server_lib)

add_camera_test(test_apicontroller web.apicontroller web/test_apicontroller.cpp web_lib)

# Qt client. Only registered when Qt was found; camera_platform_qml carries
# CameraBackend.cpp and its moc output, so the test sees the same code the app
# links. Qt6::Network is needed for QNetworkAccessManager in the public header.
if(CAMERA_PLATFORM_QT_ENABLED)
    add_camera_test(test_camerabackend client.camerabackend
                    client-qt/test_camerabackend.cpp camera_platform_qml)
    target_link_libraries(test_camerabackend PRIVATE Qt6::Network)
endif()
