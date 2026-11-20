# Changelog

What changed in each release of P31 Plugins. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/). Each release's page on GitHub
carries its section from here plus the list of its commits.

## [Unreleased]

## [0.1.6] - 2026-11-20

### Fixed

- The standalone apps get system audio about 9 ms sooner: the capture asks for
  a 128-frame buffer instead of the default 512. The other ~23 ms is a buffer
  macOS keeps for its own capture, which apps cannot change.
- P31 Meter lights the clip indicator at exactly full scale. Clipped audio sits
  at 0 dBFS and never goes over, so in sample-peak mode the light never came on.
- P31 Meter reads NaN and infinite samples as silence. A single one used to
  stop correlation, balance, loudness and true peak until the plugin restarted.

## [0.1.5] - 2026-11-17

### Added

- Auto gain in P31 Spectrum and P31 Spectrogram: the top of the display follows
  the material and the dB labels stay true. Off by default.

### Changed

- Settings open in a window of their own beside the editor instead of covering
  it. P31 Suite has one settings window, with a tab per visible panel.
- The installer lists plugins and formats as one flat list of checkboxes.

## [0.1.4] - 2026-11-16

### Added

- P31 Meter: peak, RMS, loudness, true peak, correlation and balance, ported
  from the foobar2000 Level Meter.
- P31 Suite: P31 Spectrum, Scope, Spectrogram and Meter as panels of one
  window, which can be resized, rearranged and hidden.

## [0.1.3] - 2026-11-16

### Added

- P31 Spectrogram: a scrolling spectrogram with colormaps, a frequency axis, a
  dB scale and a probe.
- The standalone apps can analyse the audio the Mac is playing (macOS 14.2 or
  later). They open no audio device while doing so.
- An icon for each standalone app.

### Changed

- Theme colours are drawn in Display P3, with retuned P3 amber colours in
  P31 Scope.
- New default settings for P31 Spectrum and P31 Scope.
- The analyzer views draw on whole device pixels, for sharper lines.

### Fixed

- P31 Scope's horizontal meter is centred on the scope.

## [0.1.2] - 2026-10-24

### Added

- P31 Spectrum: more frequency labels and a Show legend setting.
- P31 Scope: themes matching P31 Spectrum, phosphors first.

### Fixed

- P31 Spectrum analyzer and drawing fixes from the review of the foobar2000
  component.

## [0.1.1] - 2026-10-18

### Changed

- The installer lets you choose plugins and formats separately.

## [0.1.0] - 2026-10-18

### Added

- P31 Spectrum and P31 Scope as CLAP, VST3 and Audio Unit plugins and
  standalone apps, ported from
  [fb2k-components-mac-suite](https://github.com/JendaT/fb2k-components-mac-suite).
  Audio passes through untouched.
- P31 Spectrum: live and peak curves with a hover readout, phosphor themes,
  adjustable slope and resolution.
- P31 Scope: a vectorscope with a band-coloured trace and meters.
- Presets, Freeze and Gain, and an About dialog in both.
- An installer package.

[Unreleased]: https://github.com/Scannou/p31-plugins/compare/v0.1.6...HEAD
[0.1.6]: https://github.com/Scannou/p31-plugins/compare/v0.1.5...v0.1.6
[0.1.5]: https://github.com/Scannou/p31-plugins/compare/v0.1.4...v0.1.5
[0.1.4]: https://github.com/Scannou/p31-plugins/compare/v0.1.3...v0.1.4
[0.1.3]: https://github.com/Scannou/p31-plugins/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/Scannou/p31-plugins/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/Scannou/p31-plugins/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/Scannou/p31-plugins/releases/tag/v0.1.0
