# p31-plugins

Audio analyzer plugins for macOS, ported from the foobar2000 components in
[fb2k-components-mac-suite](https://github.com/Scannou/fb2k-components-mac-suite):

- **P31 Spectrum** — spectrum analyzer
- **P31 Scope** — vectorscope / goniometer with tri-band phase correlation

Both are CLAP plugins, wrapped by [clap-wrapper](https://github.com/free-audio/clap-wrapper)
into VST3, AUv2 and a standalone app. Audio passes through untouched with zero latency.

Status: phase 6. Both plugins are feature complete for macOS (settings page,
presets, settings saved with the project), with CI and a signed, notarized
installer once signing credentials are set up (see Releases).

- P31 Scope: goniometer / Lissajous, afterglow, tri-band correlation meter.
- P31 Spectrum: overlapping windowed FFT (vDSP, pffft fallback), log /
  compact-bass log / linear frequency axis, RMS or attack/decay smoothing,
  shadow and peak lines, curve or bar display.

## Using the plugins

- **Settings**: hover over the editor and click the sliders icon (top right), or
  right-click anywhere. Everything is saved with the project.
- **Presets**: the Presets button on the settings page (or the right-click
  menu) has factory presets, your own presets (saved in
  `~/Library/Audio/Presets/P31/<plugin>/`) and "Use Current Settings for New
  Instances". Loading a preset keeps the instance's light / dark appearance.
- **Parameters** (automatable): *Freeze* holds the picture still, *Gain*
  (±24 dB) changes the level the analyzer sees. Audio always passes through
  untouched.
- **Appearance**: follow the system, or fixed light or dark per instance.

## Install

Run `P31-Plugins-<version>.pkg` (from the GitHub release, or `scripts/package.sh`).
"Customize" picks the formats:

| Choice | Installs to |
|---|---|
| CLAP | `/Library/Audio/Plug-Ins/CLAP` |
| VST3 | `/Library/Audio/Plug-Ins/VST3` |
| Audio Unit | `/Library/Audio/Plug-Ins/Components` |
| Standalone apps | `/Applications` |

To uninstall, delete `P31 Spectrum.*` and `P31 Scope.*` from those folders, then
`pkgutil --pkgs | grep com.scannou.p31 | xargs -n1 sudo pkgutil --forget`.

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
width and height follow the seconds; a trailing `settings [scroll]` opens the
settings page. `P31_STATE=<file>` loads settings text (`key=value` lines) as
the plugin state first, `P31_SELECT=<item>` picks a settings pop-up item, and
`P31_DUMP_STATE=1` prints the saved state at the end.

## Validate

```sh
scripts/validate.sh [build-dir]
```

Runs clap-validator, the Steinberg VST3 validator, `auval -strict` and pluginval
(strictness 10, including GUI tests) against both plugins.

## Releases

```sh
scripts/package.sh            # Release build -> dist/P31-Plugins-<version>.pkg
```

Without signing variables the bundles are ad-hoc signed and the package is
unsigned: fine on this Mac, not for anyone else. For a distributable package:

1. In the Apple Developer account, create a **Developer ID Application** and a
   **Developer ID Installer** certificate and install both in the login keychain.
2. Store notarization credentials once:
   `xcrun notarytool store-credentials p31-notary --apple-id <id> --team-id <TEAMID>`
   (with an app-specific password).
3. Build:
   ```sh
   P31_SIGN_ID="Developer ID Application: <Name> (<TEAMID>)" \
   P31_INSTALLER_ID="Developer ID Installer: <Name> (<TEAMID>)" \
   P31_NOTARY_PROFILE=p31-notary scripts/package.sh
   ```

The package is signed, notarized and stapled.

CI (`.github/workflows/ci.yml`) builds, tests, validates and packages every push
and pull request; the unsigned package is attached to each run. Pushing a tag
`v<version>` (matching `project(... VERSION ...)` in CMakeLists.txt) also signs,
notarizes and publishes a GitHub release. It needs these repository settings:

| Kind | Name | Value |
|---|---|---|
| Variable | `P31_SIGN_ID` | `Developer ID Application: <Name> (<TEAMID>)` |
| Variable | `P31_INSTALLER_ID` | `Developer ID Installer: <Name> (<TEAMID>)` |
| Secret | `DEVELOPER_ID_APP_P12_BASE64` | `base64 -i app.p12` of the exported certificate + key |
| Secret | `DEVELOPER_ID_INSTALLER_P12_BASE64` | the same for the installer certificate |
| Secret | `P12_PASSWORD` | password of both .p12 files |
| Secret | `NOTARY_KEY_P8_BASE64` | `base64 -i AuthKey_XXXX.p8` (App Store Connect API key, Developer role) |
| Secret | `NOTARY_KEY_ID` | the key's ID |
| Secret | `NOTARY_ISSUER` | the issuer ID |

## Layout

```
core/            C++20 DSP; only platform code is the vDSP FFT backend (tests in core/tests)
render/          AppKit views, compiled into each plugin (unique ObjC class names)
plugins/common/  shared CLAP shell (ports, state, pass-through, editor hosting)
plugins/spectrum/
plugins/scope/
dev/             developer tools (p31-snapshot)
cmake/ scripts/  build helpers; bootstrap, validate, package, notices
.github/         CI
libs/            submodules (CLAP, clap-wrapper, VST3 SDK, AudioUnitSDK, RtAudio, RtMidi, pffft, doctest)
```

## License

MIT, see [LICENSE](LICENSE). Bundled SDKs: CLAP, clap-wrapper and the VST3 SDK (3.8+)
are MIT; Apple's AudioUnitSDK is Apache 2.0; RtAudio and RtMidi use MIT-style licenses;
pffft uses the BSD-style FFTPACK license; doctest (tests only) is MIT.
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) (regenerate with
`scripts/notices.sh` after updating a submodule) ships inside every bundle and
in the installer.
