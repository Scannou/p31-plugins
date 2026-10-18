p31_add_plugin(
    NAME p31-spectrum
    ID "com.scannou.p31.spectrum"
    DISPLAY_NAME "P31 Spectrum"
    AU_SUBTYPE "P31S"
    ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/src/Entry.cpp"
    SOURCES src/SpectrumPlugin.cpp src/SpectrumEditor.mm
    LINK p31-render-spectrum
)
