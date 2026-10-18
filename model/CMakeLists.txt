# Plain value types (no Qt, no Gst) describing what the platform is doing.
# Header-only today; the target exists so consumers link one name.
add_library(model_lib INTERFACE)

# Headers carry root-relative includes ("model/JsonUtil.hpp", now also
# "common/Protocol.hpp" from Profile.hpp), so consumers get the repo root
# transitively instead of each one adding it by hand.
target_include_directories(model_lib INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}
)
