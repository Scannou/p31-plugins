p31_add_plugin(
    NAME p31-scope
    ID "com.scannou.p31.scope"
    DISPLAY_NAME "P31 Scope"
    AU_SUBTYPE "P31V"
    ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/src/Entry.cpp"
    SOURCES src/ScopePlugin.cpp src/ScopeEditor.mm
    LINK p31-render-scope
)
