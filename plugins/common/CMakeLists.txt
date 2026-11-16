add_library(p31-plugin-common STATIC src/PluginBase.cpp src/SystemAudio.mm)
target_include_directories(p31-plugin-common PUBLIC include)
target_link_libraries(p31-plugin-common PUBLIC clap p31-core "-framework CoreAudio" "-framework Foundation"
                      PRIVATE p31-warnings)
target_compile_options(p31-plugin-common PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
p31_cxx20(p31-plugin-common)
