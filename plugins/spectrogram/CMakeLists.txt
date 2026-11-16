p31_add_plugin(
    NAME p31-spectrogram
    ID "com.scannou.p31.spectrogram"
    DISPLAY_NAME "P31 Spectrogram"
    AU_SUBTYPE "P31G"
    ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/src/Entry.cpp"
    SOURCES src/SpectrogramPlugin.cpp src/SpectrogramEditor.mm
    LINK p31-render-spectrogram
)
