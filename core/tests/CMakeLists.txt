add_executable(p31-core-tests
    main.cpp
    AudioRingTests.cpp
    PassThroughTests.cpp
    PresetStoreTests.cpp
    StateCodecTests.cpp
    scope/PhosphorBufferTests.cpp
    scope/ScopeLayoutTests.cpp
    scope/StereoAnalysisTests.cpp
    spectrum/BandMapTests.cpp
    spectrum/FftTests.cpp
    spectrum/SpectrumAnalyzerTests.cpp
    spectrum/StftTests.cpp
)
target_link_libraries(p31-core-tests PRIVATE p31-core doctest::doctest p31-warnings)
p31_cxx20(p31-core-tests)

add_test(NAME p31-core-tests COMMAND p31-core-tests)
