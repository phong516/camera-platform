# Plain value types (no Qt, no Gst) describing what the platform is doing.
# Header-only today; the target exists so consumers link one name.
add_library(model_lib INTERFACE)

target_include_directories(model_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
