# p31-plugins

Audio analyzer plugins for macOS, ported from the foobar2000 components in
[fb2k-components-mac-suite](https://github.com/Scannou/fb2k-components-mac-suite):

- **P31 Spectrum** — spectrum analyzer
- **P31 Scope** — vectorscope / goniometer with tri-band phase correlation

Both are CLAP plugins, wrapped by [clap-wrapper](https://github.com/free-audio/clap-wrapper)
into VST3, AUv2 and a standalone app. Audio passes through untouched with zero latency.

Status: phase 4. Both plugins work with the fb2k components' default settings;
a settings UI and saved settings come in phase 5.

- P31 Scope: goniometer / Lissajous, afterglow, tri-band correlation meter.
- P31 Spectrum: overlapping windowed FFT (vDSP, pffft fallback), log /
  compact-bass log / linear frequency axis, RMS or attack/decay smoothing,
  shadow and peak lines, curve or bar display.

## Requirements

- macOS 12+ (builds universal arm64 + x86_64)
- Xcode command line tools, CMake 3.25+, Ninja: `brew install cmake ninja`
- For validation: pluginval (`brew install --cask pluginval`)

## Build

```sh
scripts/bootstrap.sh          # submodules + clap-validator into tools/
cmake --preset dev            # Debug, native arch, build/dev
cmake --build --preset dev
```

`cmake --preset release` builds Release universal binaries into `build/release`.

Bundles land in `<build>/plugins/`. With `P31_COPY_AFTER_BUILD=ON` (default for
`dev`) they are also copied to `~/Library/Audio/Plug-Ins/{CLAP,VST3,Components}`.

In CLion: open the folder, enable the `dev` CMake preset, build `p31-spectrum_all`
or `p31-scope_all`.

## Test

```sh
ctest --preset dev                                   # core unit tests (doctest)
cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan
cmake --preset asan && cmake --build --preset asan && ctest --preset asan
```

## Look at an editor without a DAW

```sh
build/dev/dev/p31-snapshot "build/dev/plugins/P31 Scope.clap" scope.png mix 2
```

Loads the plugin, opens its editor, plays a test signal for the given seconds
and saves the editor as PNG. Signals: `mix`, `mono`, `left`, `wide`, `antiphase`,
`silence` (for the scope), `sine`, `tones`, `white` (for the spectrum). Optional
width and height follow the seconds.

## Validate

```sh
scripts/validate.sh [build-dir]
```

Runs clap-validator, the Steinberg VST3 validator, `auval -strict` and pluginval
(strictness 10, including GUI tests) against both plugins.

## Layout

```
core/            C++20 DSP; only platform code is the vDSP FFT backend (tests in core/tests)
render/          AppKit views, compiled into each plugin (unique ObjC class names)
plugins/common/  shared CLAP shell (ports, state, pass-through, editor hosting)
plugins/spectrum/
plugins/scope/
dev/             developer tools (p31-snapshot)
cmake/ scripts/
libs/            submodules (CLAP, clap-wrapper, VST3 SDK, AudioUnitSDK, RtAudio, RtMidi, pffft, doctest)
```

## License

MIT, see [LICENSE](LICENSE). Bundled SDKs: CLAP, clap-wrapper and the VST3 SDK (3.8+)
are MIT; Apple's AudioUnitSDK is Apache 2.0; RtAudio and RtMidi use MIT-style licenses;
pffft uses the BSD-style FFTPACK license; doctest (tests only) is MIT.
Shipping binaries must include their notices (handled in phase 6).
