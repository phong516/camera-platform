add_executable(camera-platform
    main.cpp
    Application.cpp
)

target_include_directories(camera-platform PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(camera-platform PRIVATE 
    video_lib
)