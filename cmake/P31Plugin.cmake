# Shared compile options and the per-plugin assembly helper.

add_library(p31-warnings INTERFACE)
target_compile_options(p31-warnings INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
)

# Shown as the creator/manufacturer in hosts (CLAP vendor, VST3 vendor, AU name).
set(P31_VENDOR "P31")

# Compile a target of ours as C++20 without exporting the requirement.
function(p31_cxx20 target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 20 OBJCXX_STANDARD 20)
endfunction()

# p31_add_plugin(NAME <name> ID <clap id> DISPLAY_NAME <name> AU_SUBTYPE <4cc>
#                ENTRY <entry.cpp> SOURCES <impl sources...>)
#
# Builds <NAME>-impl (static lib holding the whole plugin) and, through
# clap-wrapper's make_clapfirst_plugins, <NAME>_clap/_vst3/_auv2/_standalone
# plus the umbrella <NAME>_all target.
function(p31_add_plugin)
    cmake_parse_arguments(P "" "NAME;ID;DISPLAY_NAME;AU_SUBTYPE;ENTRY" "SOURCES" ${ARGN})

    set(impl ${P_NAME}-impl)
    add_library(${impl} STATIC ${P_SOURCES})
    target_link_libraries(${impl} PUBLIC p31-plugin-common PRIVATE p31-warnings)
    p31_cxx20(${impl})
    target_compile_definitions(${impl} PRIVATE
        P31_PLUGIN_ID="${P_ID}"
        P31_PLUGIN_NAME="${P_DISPLAY_NAME}"
        P31_VERSION="${PROJECT_VERSION}"
        P31_VENDOR="${P31_VENDOR}"
    )

    make_clapfirst_plugins(
        TARGET_NAME ${P_NAME}
        IMPL_TARGET ${impl}
        OUTPUT_NAME "${P_DISPLAY_NAME}"
        ENTRY_SOURCE "${P_ENTRY}"

        BUNDLE_IDENTIFIER "${P_ID}"
        BUNDLE_VERSION "${PROJECT_VERSION}"

        COPY_AFTER_BUILD ${P31_COPY_AFTER_BUILD}

        PLUGIN_FORMATS CLAP VST3 AUV2
        ASSET_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"

        AUV2_MANUFACTURER_NAME "${P31_VENDOR}"
        AUV2_MANUFACTURER_CODE "Scnu"
        AUV2_SUBTYPE_CODE "${P_AU_SUBTYPE}"
        AUV2_INSTRUMENT_TYPE "aufx"

        STANDALONE_CONFIGURATIONS
            standalone "${P_DISPLAY_NAME}" "${P_ID}"
    )
endfunction()
