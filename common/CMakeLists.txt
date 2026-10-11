# Header-only value types shared by the server and every client.
# No GStreamer, no Qt: this target must stay dependency-free so that both sides
# can include it without inheriting either stack.
add_library(common INTERFACE)

target_include_directories(common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(common INTERFACE cxx_std_23)

# Documented explicitly so IDE/clangd users see the intended entry point.
set_property(TARGET common PROPERTY INTERFACE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Protocol.hpp
)
