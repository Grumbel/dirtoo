# dirtoo-hilbert-thumb

Toy **Hilbert-curve binary map** thumbnailer inspired by [CantorDust](https://github.com/google/cantordust)-style visualizations.

Maps file bytes along a Hilbert curve onto a square PNG. Zero runs stay dark;
high-entropy regions light up with a spectral palette. Useful for executables,
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
dirtoo-hilbert-thumb <input> <output.png> [size]
```

`size` is the square edge in pixels (default **128**, max **1024**). The first
16 MiB of the file are used (enough for a dense map at typical thumbnail sizes).

## XDG thumbnailer (Thumbnailer1 / tumbler)

Installing the package drops:

```text
share/thumbnailers/hilbert-curve.thumbnailer
```

with `Exec=dirtoo-hilbert-thumb %i %o %s`. File managers that use the FreeDesktop
thumbnailer service (including dirtoo via Thumbnailer1 D-Bus) will pick it up for
the listed MIME types after install + session restart (or `pkill tumblerd` /
thumbnailer daemon reload).

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

From the `dirtoo/` flake:

```bash
nix build .#hilbert-thumbnailer
nix shell .#hilbert-thumbnailer -c dirtoo-hilbert-thumb --help
```
