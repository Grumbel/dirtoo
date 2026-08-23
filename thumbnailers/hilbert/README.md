# dirtoo-hilbert-thumb

Toy **Hilbert-curve binary map** thumbnailer inspired by [CantorDust](https://github.com/google/cantordust)-style visualizations.

Maps file bytes along a Hilbert curve onto a square PNG. Zero runs stay dark;
byte values are colored by a selectable palette. Useful for executables,
firmware, disk images, and other opaque binaries.

## Build (standalone)

```bash
cmake -S . -B build -G Ninja
cmake --build build
# optional: cmake --install build --prefix ~/.local
```

Dependencies: C++20 compiler, zlib, CMake ≥ 3.20.

## CLI

```text
dirtoo-hilbert-thumb [options] <input> <output.png> [size]

  --palette NAME / -p NAME   Color map (default: spectrum)
  -h, --help
  size                       Square edge in pixels (default 128, max 1024)
```

The first 16 MiB of the file are used (enough for a dense map at typical
thumbnail sizes).

### Palettes

| Name | Behavior |
|------|----------|
| **spectrum** | Original cool→green→red ramp. Readable on binaries; mid-range green dominates high-entropy / compressed data. |
| **gray** | Grayscale intensity of the byte. |
| **rgb** | High bits split across R/G/B channels — multicolored noise without a preferred hue. |
| **hsv** | Hue = byte/255, fixed saturation/value (full color wheel). |
| **viridis** | Approx. viridis-style ramp (purple→teal→yellow); flatter midtones. |

Aliases: `grey`/`grayscale` → gray; `bits`/`channels` → rgb; `hue` → hsv;
`flat`/`balanced` → viridis; `turbo`/`default` → spectrum.

Example (neutral look on compressed data):

```bash
dirtoo-hilbert-thumb -p hsv archive.tar.gz /tmp/out.png 256
```

## XDG thumbnailer (Thumbnailer1 / tumbler)

Installing the package drops:

```text
share/thumbnailers/hilbert-curve.thumbnailer
```

with `Exec=dirtoo-hilbert-thumb %i %o %s`. File managers that use the FreeDesktop
thumbnailer service (including dirtoo via Thumbnailer1 D-Bus) will pick it up for
the listed MIME types after install + session restart (or `pkill tumblerd` /
thumbnailer daemon reload).

Note: the `.thumbnailer` entry does not pass `--palette`; the default **spectrum**
is used. Override only when invoking the CLI directly, or customize the Exec line.

### Writing your own thumbnailer

1. CLI: `tool INPUT OUTPUT SIZE` (paths and pixel size).
2. Install a `*.thumbnailer` under `$prefix/share/thumbnailers/`:

   ```ini
   [Thumbnailer Entry]
   TryExec=my-tool
   Exec=my-tool %i %o %s
   MimeType=application/x-mine;
   ```

3. Output a square PNG; the service caches it under `~/.cache/thumbnails/`.

This directory is intentionally **not** linked against dirtoo libraries so it can
ship as a separate flake package.

## Nix

From the repository flake:

```bash
nix build .#hilbert-thumbnailer
nix shell .#hilbert-thumbnailer -c dirtoo-hilbert-thumb --help
```
