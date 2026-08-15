# dirtoo-text-thumb

Layout-preview thumbnailer for text files: **white background**, **black** tiny
bitmap glyphs, up to **three columns** (~80 characters wide).

| Columns | Content |
|---------|---------|
| 1 | Start of the file (short documents) |
| 2 | Start + end |
| 3 | Start + middle + end |

Glyphs are intentionally tiny — the goal is a **gestalt of structure** (line
density, blank regions), not readable body text.

## Build

```bash
cmake -S . -B build && cmake --build build
```

Dependencies: C++20, zlib, CMake ≥ 3.20.

## CLI

```text
dirtoo-text-thumb <input> <output.png> [size]
```

## Nix

```bash
nix build .#text-thumbnailer
```
