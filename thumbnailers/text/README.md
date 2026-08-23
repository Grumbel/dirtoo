# dirtoo-text-thumb

Layout-preview thumbnailer for text files using **Qt `QPainter`**.

Up to **three columns** (~80 characters) show the **start / middle / end** of
the file. Default typeface is **DejaVu Sans Mono**, with Qt/fontconfig
fallbacks. Defaults target a readable 128×128 preview (supersample 4, font
size 2, warm paper colors).

## Why Qt?

The first cut used FreeType directly to stay dependency-light (like the Hilbert
thumbnailer). **QPainter** is a better fit alongside dirtoo: the same font
stack as the desktop, built-in antialiasing, smooth supersampled downscale via
`QImage::scaled`, and no hand-rolled glyph blending.

## CLI

```text
dirtoo-text-thumb [options] <input> <output.png> [size]

  --font FAMILY      font family              (default: DejaVu Sans Mono)
  --weight NAME      light|regular|medium|bold
  --font-size F      pixel size before --ss   (default: 2; fractional OK)
  --fg / --bg COLOR  #rgb / #rrggbb / R,G,B   (default #292720 / #F5F0D8)
  --ss N             supersample 1–8         (default: 4)
  --size N           thumbnail edge pixels   (default: 128)
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
