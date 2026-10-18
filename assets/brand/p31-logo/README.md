# P31 logo — direction A (Lissajous 3:1)

Single colour. Every file paints with `currentColor`, so the theme decides the colour.
The default phosphor green is `#4CFF7C`.

## Which file where

| File | Use at | Notes |
|---|---|---|
| `p31-lockup.svg` | Settings panel header, 24 px tall (70×24) | small mark + "P31" wordmark; put "Spectrum" / "Scope" and the version in UI text next to it |
| `p31-mark.svg` | 48 px and up (About dialog, app icon) | ring + figure |
| `p31-mark-small.svg` | 20–40 px | figure only, heavier line |
| `p31-mark-16.svg` | 16 px | figure only, heaviest line |
| `p31-wordmark.svg` | lettering on its own | 208×112 |

## Two variants of each file

- `outline/` — strokes converted to filled paths (`fill="currentColor"`). **Use these in the plugin**: no stroke rendering, so they scale identically in every renderer.
- `stroke/` — the editable source (stroked paths, `stroke="currentColor"`). Keep these for future edits.

To recolour in code, replace `currentColor` with the theme colour (or set the
fill on the parsed drawable).

The glow from the design canvas is not baked in. If it's wanted, draw it at
render time (a blurred copy of the same path underneath, in the same colour at
lower alpha).

Keep at least the mark's ring radius × 0.25 of clear space around the lockup;
don't stretch, rotate, or add a second colour.

## readme/

`p31-lockup-dark.svg` and `p31-lockup-light.svg`: the outline lockup with the
colour filled in (`#4CFF7C` and `#1B8A3A`), for the project README. GitHub
shows SVGs as images, where `currentColor` would come out black.
