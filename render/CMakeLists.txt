# AppKit rendering. INTERFACE libraries carrying their sources: each plugin
# compiles them itself, with its own P31_OBJC_SUFFIX, so the Objective-C class
# names never clash between plugins loaded into one host (see ObjCNames.h).

# The brand logo's outline SVGs, embedded as strings (LogoData.h).
set(p31_logo_dir ${PROJECT_SOURCE_DIR}/assets/brand/p31-logo/outline)
file(READ ${p31_logo_dir}/p31-lockup.svg P31_LOGO_LOCKUP)
file(READ ${p31_logo_dir}/p31-mark.svg P31_LOGO_MARK)
configure_file(common/LogoData.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/p31/render/LogoData.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${p31_logo_dir}/p31-lockup.svg ${p31_logo_dir}/p31-mark.svg)

add_library(p31-render-common INTERFACE)
target_sources(p31-render-common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/AnimatedView.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/EditorRootView.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/Logo.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/Menus.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/SettingsPanel.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/common/src/SettingsWindow.mm
)
target_include_directories(p31-render-common INTERFACE common/include ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(p31-render-common INTERFACE p31-core "-framework Cocoa" "-framework QuartzCore")

add_library(p31-render-scope INTERFACE)
target_sources(p31-render-scope INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/scope/src/ScopeView.mm
)
target_include_directories(p31-render-scope INTERFACE scope/include)
target_link_libraries(p31-render-scope INTERFACE p31-render-common p31-core)

add_library(p31-render-spectrum INTERFACE)
target_sources(p31-render-spectrum INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/spectrum/src/SpectrumView.mm
)
target_include_directories(p31-render-spectrum INTERFACE spectrum/include)
target_link_libraries(p31-render-spectrum INTERFACE p31-render-common p31-core)

add_library(p31-render-meter INTERFACE)
target_sources(p31-render-meter INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/meter/src/MeterRenderer.mm
    ${CMAKE_CURRENT_SOURCE_DIR}/meter/src/MeterView.mm
)
target_include_directories(p31-render-meter INTERFACE meter/include)
target_link_libraries(p31-render-meter INTERFACE p31-render-common p31-core)

add_library(p31-render-suite INTERFACE)
target_sources(p31-render-suite INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/suite/src/SuiteView.mm
)
target_include_directories(p31-render-suite INTERFACE suite/include)
target_link_libraries(p31-render-suite INTERFACE p31-render-common p31-core)

add_library(p31-render-spectrogram INTERFACE)
target_sources(p31-render-spectrogram INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/spectrogram/src/SpectrogramView.mm
)
target_include_directories(p31-render-spectrogram INTERFACE spectrogram/include)
target_link_libraries(p31-render-spectrogram INTERFACE p31-render-common p31-core)
