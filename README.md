# dirtoo

dirtoo is a local file manager for Linux, built for people who spend a lot of
time in folders full of photos, videos, and other media.

It aims to make “find the thing, look at it, move on” feel quick: a filter
language that actually understands sizes and durations, proper thumbnails,
tags that stick to file content (not just the path), and the usual copy/paste
and drag-and-drop you expect from a desktop file manager.

The app is under active development. Don’t treat it as the only tool for
irreplaceable data. You’ll see a one-time warning at startup; you can dismiss
it permanently once you’re comfortable.

**License:** GPL-3.0-or-later (REUSE-style SPDX headers on sources).

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
size:1M..50M
tag:work OR tagged:no
type:image checksummed:yes tagged:no
contains:hello
aspect:16:9
```

Ranges use `lo-hi` or `lo..hi` (e.g. `duration:3-10m` is three to ten minutes
when the unit is only on the high end). **Help → Filter expression help** in the
app documents the full language; `dt-filter --help` covers the same from the CLI.

**QuickFilter chips** under the listing offer one-click type and tag filters
(including untagged helpers when useful). Pin expressions you use often; pins
can be limited to a folder or a whole subtree.

**Recursive search** (**F3** / **Ctrl+F**) uses the same language when the file
might sit deeper under the current location.

**Tags** follow content identity (checksum), so renaming or moving does not
drop them. Tag from the context menu or **Ctrl+T**, manage definitions in Tag
Manager, and open “everything with this tag” from there.

**Thumbnails** use the desktop’s freedesktop thumbnailer when available.
Folders can show a montage of children. Force a reload when something looks
stale.

**Archives** (zip, tar, 7z, rar, and similar) open **read-only** for browsing;
members can be extracted on demand for thumbnails and tagging.

**Places, bookmarks, and devices** sit in the sidebar. Bookmarks are grouped
under their own heading. Toggle the current location from the location bar
button or **Ctrl+D**.

**Clipboard and drag-and-drop** work the usual way. Transfers can run in the
background; conflict handling is available when targets already exist.

Also: Detail / Icons / Small icons views, zoom and crop/letterbox thumbnails,
media badges, history, multi-window (**Ctrl+N**, middle-click), read-only mode
(**Ctrl+Shift+R**), save visible file list (**Ctrl+Shift+S**), Properties,
Open With…, and `dt-*` CLI helpers in the same tree.

### Not in scope right now

Writing into archives, remote filesystems (SMB, SFTP, …), and a full undo stack.

---

## Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| **F2** | Rename |
| **F3** / **Ctrl+F** | Recursive search |
| **Ctrl+T** | Tag selection |
| **Ctrl+D** | Toggle bookmark for this location |
| **Alt+Return** | Properties |
| **F5** | Refresh |
| **Backspace** / **Alt+Up** | Parent directory |
| **Alt+Home** | Home |
| **Alt+Left** / **Alt+Right** | History back / forward |
| **Ctrl+L** | Focus location bar |
| **Ctrl+C** / **X** / **V** | Copy / Cut / Paste |
| **Delete** | Delete selection |
| **Ctrl++** / **Ctrl+-** | Zoom icons |
| **Ctrl+N** | New window |
| **Ctrl+Shift+R** | Toggle read-only mode |
| **Ctrl+Shift+S** | Save file list as… |
| **Escape** | Clear selection / dismiss overlays |

With the file view focused, typing jumps via type-ahead (or goes to the filter
bar when that bar is open).

---

## Build and run

From the repository root:

```bash
cmake -B build -G Ninja
cmake --build build
./build/apps/dirtoo/dirtoo
```

Optional:

```bash
ctest --test-dir build
cmake --install build    # binaries + man pages
```

With Nix:

```bash
nix develop
cmake -B build -G Ninja && cmake --build build
```

`nix build` builds the package without running tests. `nix flake check` runs
the unit tests using the already-built binary when it is in the store.

CLI tools (`dt-copy`, `dt-filter`, `dt-mediainfo`, `dt-tag`, …) land next to the
GUI binary; use `--help` or the installed man pages.

---

## Repository layout

| Path | Purpose |
|------|---------|
| `apps/dirtoo/` | Qt6 GUI |
| `libs/dirops/` | Copy / move / rename / delete / mkdir |
| `libs/dirtoo-fs/` | Locations, file info, listing |
| `libs/dirtoo-collection/` | Sort, filter, group |
| `libs/dirtoo-filter/` | Filter language + media metadata |
| `libs/dirtoo-hash/` | Checksums + SQLite cache |
| `libs/dirtoo-tags/` | Tag definitions and associations |
| `libs/dirtoo-watcher/` | Directory change notifications |
| `libs/dirtoo-thumbnail/` | Thumbnailer client |
| `libs/dirtoo-archive/` | Read-only archive access |
| `tools/` | `dt-*` CLI helpers |
| `tests/` | Catch2 tests |
| `AGENTS.md` / `TODO.md` | Contributor rules and open work |

The older **Python prototype** lives in a separate repository:
[Grumbel/dirtoo-py](https://github.com/Grumbel/dirtoo-py.git). It is a behavioral
reference only—not part of this build.

---

## License

**GPL-3.0-or-later**.
