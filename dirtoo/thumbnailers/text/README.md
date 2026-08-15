# dirtoo-text-thumb

Layout-preview thumbnailer for text files using **FreeType + Fontconfig**.

Up to **three columns** (~80 characters) show the **start / middle / end** of
the file. Glyphs are intentionally small — a density gestalt, not readable copy.

**Default font:** `JetBrains Mono` (falls back through IBM Plex Mono, DejaVu
Sans Mono, Liberation Mono, …).

## CLI

```text
dirtoo-text-thumb [options] <input> <output.png> [size]

  --font FAMILY      font family or path to .ttf/.otf  (default: JetBrains Mono)
  --weight NAME      light|regular|medium|bold         (default: regular)
  --font-size N      glyph pixel size before --ss      (default: 6)
  --fg COLOR         text color                        (default: #000000)
  --bg COLOR         background                        (default: #ffffff)
  --ss N             supersample 1–8                   (default: 4)
  --size N           thumbnail edge                    (default: 128)
```

`COLOR`: `#rgb`, `#rrggbb`, or `R,G,B`.

### Examples

```bash
dirtoo-text-thumb README.md out.png 128
dirtoo-text-thumb --font "IBM Plex Mono" --weight light --font-size 5 notes.txt out.png
dirtoo-text-thumb --font /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf code.cpp out.png
```

## Environment

| Variable | Meaning |
|----------|---------|
| `DIRTOO_TEXT_THUMB_FONT` | Family or file path |
| `DIRTOO_TEXT_THUMB_WEIGHT` | Weight name |
| `DIRTOO_TEXT_THUMB_FONT_SIZE` | Pixel size |
| `DIRTOO_TEXT_THUMB_FG` / `_BG` | Colors |
| `DIRTOO_TEXT_THUMB_SS` | Supersample |
| `DIRTOO_TEXT_THUMB_SIZE` | Default edge size |

## Build

```bash
cmake -S . -B build && cmake --build build
```

Dependencies: C++20, zlib, FreeType, Fontconfig, CMake ≥ 3.20.

## Nix

```bash
nix build .#text-thumbnailer
```
