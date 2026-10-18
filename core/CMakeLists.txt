# Pure C++20, no platform or plugin-format dependencies.
add_library(p31-core STATIC src/AudioRing.cpp)
target_include_directories(p31-core PUBLIC include)
target_link_libraries(p31-core PRIVATE p31-warnings)
p31_cxx20(p31-core)

if (P31_BUILD_TESTS)
    add_subdirectory(tests)
endif()
