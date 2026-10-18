# Pure C++20, no platform or plugin-format dependencies. Consumers compile
# as C++20 via p31_cxx20().
add_library(p31-core INTERFACE)
target_include_directories(p31-core INTERFACE include)
