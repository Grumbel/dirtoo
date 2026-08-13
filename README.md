# dirtoo

dirtoo is a local file manager for Linux, built for people who spend a lot of
time in folders full of photos, videos, and other media.

It aims to make “find the thing, look at it, move on” feel quick: a filter
language that actually understands sizes and durations, proper thumbnails,
tags that stick to file content (not just the path), and the usual
copy/paste and drag-and-drop you expect from a desktop file manager.

The app is under active development. Don’t treat it as the only tool for
irreplaceable data. You’ll see a one-time warning at startup; you can dismiss
it permanently once you’re comfortable.

**License:** GPL-3.0-or-later

---

## Who it’s for

- Sorting and reviewing large image or video directories
- Finding files by size, type, duration, aspect ratio, or content tags
- Working with archives without unpacking everything first
- Preferring keyboard shortcuts and a filter bar over endless scrolling

If you mainly need a simple “two panes and a tree,” a more conventional file
manager may be a better fit. dirtoo leans toward media and filtering.

---

## Highlights

**Filter the current folder** as you type. Expressions can combine globs,
sizes, media attributes, and tags, for example:

```text
type:image size:>2M
duration:3-10m
tag:work OR tagged:no
type:image checksummed:yes tagged:no
```

**QuickFilter chips** appear under the listing (types, tags, untagged helpers).
Pin expressions you use often; pins can be limited to a folder or a whole subtree.

**Recursive search** uses the same language when the file might be nested
deeper under the current location.

**Tags** follow the file’s content identity (checksum), so renaming or moving
doesn’t drop them. Tag files from the context menu or **Ctrl+T**, manage
definitions in Tag Manager, and open “everything with this tag” from there.

**Thumbnails** use the desktop’s freedesktop thumbnailer when available.
Folders can show a montage of children. You can force a reload when something
looks stale.

**Archives** (zip, tar, 7z, and similar) open read-only for browsing; members
can be extracted on demand for thumbnails and tagging.

**Places, bookmarks, and devices** sit in the sidebar. Bookmarks are grouped
under their own heading so they don’t blend into Home / Documents / etc.

**Clipboard and drag-and-drop** work the usual way. Transfers can run in the
background; conflict handling is available when targets already exist.

More detail, shortcuts, and build instructions: **[dirtoo/README.md](dirtoo/README.md)**.

---

## Repository layout

| Path | What it is |
|------|------------|
| **[dirtoo/](dirtoo/)** | The C++ / Qt6 application (this is what you build and run) |
| **dirtoo-py/** | Older Python prototype, kept as a reference only |
| **AGENTS.md** / **TODO.md** | Contributor notes and open work |

---

## Build (short)

From the `dirtoo/` tree:

```bash
cmake -B build -G Ninja
cmake --build build
./build/apps/dirtoo/dirtoo
```

Or with Nix: `nix develop` inside `dirtoo/`, then the same cmake steps.
`nix flake check` runs the unit tests against the built package without
re-running them on every plain `nix build`.

See [dirtoo/README.md](dirtoo/README.md) for dependencies, tools (`dt-*`), and
keyboard shortcuts.
