# Plain C++20. The only platform code is the vDSP FFT backend, which sits
# behind p31::spectrum::RealFft next to the portable pffft one.

# pffft (BSD-style FFTPACK license): just the single-precision transform,
# not the library's own CMake project with its benchmarks and extras.
add_library(p31-pffft STATIC
    ${PROJECT_SOURCE_DIR}/libs/pffft/src/pffft.c
    ${PROJECT_SOURCE_DIR}/libs/pffft/src/pffft_common.c
)
target_include_directories(p31-pffft PUBLIC ${PROJECT_SOURCE_DIR}/libs/pffft/include/pffft)
# NEON is opt-in upstream; SSE is picked up automatically on x86_64.
target_compile_definitions(p31-pffft PUBLIC PFFFT_STATIC_DEFINE PRIVATE PFFFT_ENABLE_NEON _USE_MATH_DEFINES)
target_compile_options(p31-pffft PRIVATE -w)

add_library(p31-core STATIC
    src/AudioRing.cpp
    src/scope/PhosphorBuffer.cpp
    src/scope/StereoAnalysis.cpp
    src/spectrum/BandMap.cpp
    src/spectrum/Fft.cpp
    src/spectrum/SpectrumAnalyzer.cpp
    src/spectrum/Stft.cpp
)
target_include_directories(p31-core PUBLIC include)
target_link_libraries(p31-core PRIVATE p31-pffft p31-warnings)
if (APPLE)
    target_link_libraries(p31-core PUBLIC "-framework Accelerate")
endif()
p31_cxx20(p31-core)

if (P31_BUILD_TESTS)
    add_subdirectory(tests)
endif()
