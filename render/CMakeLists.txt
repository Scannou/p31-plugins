# AppKit rendering. INTERFACE libraries carrying their sources: each plugin
# compiles them itself, with its own P31_OBJC_SUFFIX, so the Objective-C class
# names never clash between plugins loaded into one host (see ObjCNames.h).

add_library(p31-render-common INTERFACE)
target_sources(p31-render-common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/AnimatedView.mm
)
target_include_directories(p31-render-common INTERFACE common/include)
target_link_libraries(p31-render-common INTERFACE "-framework Cocoa" "-framework QuartzCore")

add_library(p31-render-scope INTERFACE)
target_sources(p31-render-scope INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/scope/src/ScopeView.mm
)
target_include_directories(p31-render-scope INTERFACE scope/include)
target_link_libraries(p31-render-scope INTERFACE p31-render-common p31-core)
