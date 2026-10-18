# Developer tools, not shipped.
add_executable(p31-snapshot snapshot/main.mm)
target_link_libraries(p31-snapshot PRIVATE clap "-framework Cocoa" "-framework QuartzCore")
target_compile_options(p31-snapshot PRIVATE -fobjc-arc)
p31_cxx20(p31-snapshot)
