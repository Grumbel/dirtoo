# dirtoo-text-thumb

Layout-preview thumbnailer for text files: configurable **foreground /
background**, **glyph scale**, and **pixel size**. Up to **three columns**
(~80 characters) showing the **start / middle / end** of the file (fewer
columns for short documents).

Glyphs are a built-in 5×7 bitmap (not a system TTF) — dense enough for a
**gestalt of structure**, not readable body text.

## CLI

```text
dirtoo-text-thumb [options] <input> <output.png> [size]

  --fg COLOR     text color       (default #000000)
  --bg COLOR     background       (default #ffffff)
  --scale F      glyph scale      (default 0.5; range 0.25–2.0)
  --size N       same as [size]   (default 128)
```

`COLOR` may be `#rgb`, `#rrggbb`, or `R,G,B`.

### Examples

```bash
dirtoo-text-thumb README.md out.png 128
dirtoo-text-thumb --fg '#1a1a1a' --bg '#f5f0e6' --scale 0.75 notes.txt out.png 256
dirtoo-text-thumb --fg 200,220,255 --bg 16,16,24 --scale 1.0 code.cpp out.png
```

## Environment

Used when the matching flag is omitted (handy for XDG thumbnailers):

| Variable | Meaning |
|----------|---------|
| `DIRTOO_TEXT_THUMB_FG` | Foreground color |
| `DIRTOO_TEXT_THUMB_BG` | Background color |
| `DIRTOO_TEXT_THUMB_SCALE` | Glyph scale |
| `DIRTOO_TEXT_THUMB_SIZE` | Default size if not passed as `%s` |

## XDG thumbnailer

```ini
Exec=dirtoo-text-thumb %i %o %s
```

Customize by editing the installed `.thumbnailer` or exporting the env vars
above before starting the thumbnailer daemon.

## Build

```bash
cmake -S . -B build && cmake --build build
```

Dependencies: C++20, zlib, CMake ≥ 3.20.

## Nix

```bash
nix build .#text-thumbnailer
```
