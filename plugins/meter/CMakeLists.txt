p31_add_plugin(
    NAME p31-meter
    ID "com.scannou.p31.meter"
    DISPLAY_NAME "P31 Meter"
    AU_SUBTYPE "P31M"
    ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/src/Entry.cpp"
    SOURCES src/MeterPlugin.cpp src/MeterMeasurement.cpp src/MeterEditor.mm
    LINK p31-render-meter
)
