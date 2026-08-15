# dirtoo-text-thumb

Layout-preview thumbnailer for text files using **Qt `QPainter`**.

Up to **three columns** (~80 characters) show the **start / middle / end** of
the file. Default typeface is **JetBrains Mono** (compact monospace), with
Qt/fontconfig fallbacks.

## Why Qt?

The first cut used FreeType directly to stay dependency-light (like the Hilbert
thumbnailer). **QPainter** is a better fit alongside dirtoo: the same font
stack as the desktop, built-in antialiasing, smooth supersampled downscale via
`QImage::scaled`, and no hand-rolled glyph blending.

## CLI

```text
dirtoo-text-thumb [options] <input> <output.png> [size]

  --font FAMILY      font family              (default: JetBrains Mono)
  --weight NAME      light|regular|medium|bold
  --font-size N      pixel size before --ss   (default: 6)
  --fg / --bg COLOR  #rgb / #rrggbb / R,G,B
  --ss N             supersample 1–8         (default: 4)
```

Environment: `DIRTOO_TEXT_THUMB_{FONT,WEIGHT,FONT_SIZE,FG,BG,SS,SIZE}`.

## Build

```bash
cmake -S . -B build && cmake --build build
```

Dependencies: C++20, Qt6 Gui, CMake ≥ 3.20.

## Nix

```bash
nix build .#text-thumbnailer
```
