add_library(p31-plugin-common STATIC src/PluginBase.cpp)
target_include_directories(p31-plugin-common PUBLIC include)
target_link_libraries(p31-plugin-common PUBLIC clap p31-core PRIVATE p31-warnings)
p31_cxx20(p31-plugin-common)
