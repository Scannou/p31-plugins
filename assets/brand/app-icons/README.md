# App icons

One icon per standalone app. Each is the P31 mark's ring as a round CRT screen
on a dark tile, showing what the app shows, in one phosphor colour:

| File | App | Colour | On the screen |
|---|---|---|---|
| `p31-spectrum` | P31 Spectrum | P7 blue `#6FB7FF` | a spectrum curve over its filled area |
| `p31-scope` | P31 Scope | P31 green `#4CFF7C` | the mark's own 3:1 Lissajous figure |
| `p31-spectrogram` | P31 Spectrogram | P3 amber `#FFB000` | three notes, each a fundamental with its harmonics |
| `p31-meter` | P31 Meter | P4 white `#E6EDF2` | the scale of a VU face, heavier above 0 VU, and its needle |
| `p31-suite` | P31 Suite | P4 white `#E6EDF2` ring and dividers | the four panels as the app first shows them, each a small figure in its own app's colour (the meter as two level bars) |

The `.svg` is the source (1024 × 1024; the tile is 824 × 824 at 100,100, as
macOS lays app icons out). The `.icns` is what the app bundle carries: CMake
copies `<target name>.icns` into each standalone app. After editing an SVG,
run `scripts/icons.sh` (needs Inkscape) and commit both files.

The ring, the line weights and the figure come from `../p31-logo/stroke/p31-mark.svg`:
its 100 × 100 box is scaled by 6.4 into the middle of the tile.
