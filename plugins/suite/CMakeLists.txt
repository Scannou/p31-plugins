# Every analyzer in one window: the editors of the other plugins are compiled
# into this one too, beside its own.
p31_add_plugin(
    NAME p31-suite
    ID "com.scannou.p31.suite"
    DISPLAY_NAME "P31 Suite"
    AU_SUBTYPE "P31U"
    ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/src/Entry.cpp"
    SOURCES
        src/SuitePlugin.cpp
        src/SuiteEditor.mm
        ../spectrum/src/SpectrumEditor.mm
        ../scope/src/ScopeEditor.mm
        ../spectrogram/src/SpectrogramEditor.mm
        ../meter/src/MeterEditor.mm
        ../meter/src/MeterMeasurement.cpp
    LINK p31-render-suite p31-render-spectrum p31-render-scope p31-render-spectrogram p31-render-meter
)
target_include_directories(p31-suite-impl PRIVATE ../spectrum/src ../scope/src ../spectrogram/src ../meter/src)
