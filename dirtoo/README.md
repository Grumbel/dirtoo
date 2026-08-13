# dirtoo

A local file manager for Linux, aimed at media-heavy folders: filtering,
thumbnails, tags, archives, and keyboard-friendly browsing—without dropping
ordinary copy/paste and drag-and-drop.

> Still under development. Prefer not to rely on it as the only tool for
> irreplaceable data. A one-time warning appears at startup (you can dismiss
> it for good).

This is the **C++23 / Qt6** app. The older Python version in `../dirtoo-py/`
is a behavioral reference only.

---

## What you can do

### Browse

- Location bar as breadcrumb or editable path (**Ctrl+L**)
- Back / Forward, Parent, Home
- Sidebar: standard places, **Bookmarks** (separate section), removable devices
  (mount / unmount / eject when UDisks is around)
- History and recently opened files
- New window (**Ctrl+N**); middle-click a folder or breadcrumb to open another window
- **Archives** (zip, tar, tar.gz, 7z, rar, …) browse read-only by opening them

### Views

| Mode | Good for |
|------|----------|
| **Detail** | Columns, sorting, dense lists |
| **Icons** | Thumbnails and media badges |
| **Small icons** | Compact grid |

Zoom the icon views, choose crop vs letterbox thumbnails, and toggle media
badges (size, duration, resolution, and similar). Folders can show a small
montage of their contents. Hidden files and permission-restricted files get
visual hints so they don’t look identical to everything else.

### Filter (this folder only)

The filter bar narrows the current directory as you type. **Enter** keeps the
filter and returns focus to the list.

```text
*.png
size:>1M type:file
size:1M..50M
type:video duration:3-10m
tag:work
tagged:no
checksummed:yes tagged:no
contains:hello
date:2024
aspect:16:9
(a or b) and not c
```

Ranges use `lo-hi` or `lo..hi` (e.g. `duration:3-10m` is three to ten minutes
when the unit sits on the high end only). **Help → Filter expression help** in
the app has the full language; `dt-filter --help` covers the same from the CLI.
**Up / Down** in the filter field walks recent expressions.

**QuickFilter** chips above the bar offer one-click type and tag filters from
the current listing (including **Untagged** / **Untagged images** when that
makes sense). **Pin filter** saves an expression; right-click a pin to edit it,
limit it to a directory or subtree, or remove it.

### Search (recursive)

**F3** / **Ctrl+F** opens recursive search with the same expression language.
Use that when the file might live deeper under the current location—not only
in the folder you’re looking at.

### Tags

Tags are tied to a file’s content (via checksum), not only its path, so a
rename or move doesn’t invent a “new” untagged file.

- **Tag…** on the selection (context menu or **Ctrl+T**): add or remove tags;
  multiple names in one go (spaces or commas); pick from known tags or type
  with completion
- **Tag Manager**: rename definitions, edit label/color/badge, **Show files**
  for everything known with that tag (`tag://name` in the location line)
- Click a tag chip on a thumbnail to filter for that tag
- Filter helpers: `tag:name`, `tag:location-*`, `tagged:yes|no`,
  `checksummed:yes|no` (cache only—filter never hashes whole trees)

Namespaces use `:` (e.g. `game:doom`). Chips show the local part; an unprefixed
`tag:doom` matches any namespace.

### Thumbnails

Uses the desktop freedesktop thumbnailer over D-Bus when present. You can
reload thumbnails for the selection when something is wrong or missing.
Archive members are extracted on demand so they can get thumbs too. Background
work shows up in the activity indicator (counts and percentages when known).

### Files and transfers

Copy, cut, paste, delete, rename, mkdir, new empty file, and drag-and-drop work
as you’d expect. Transfers can run in the background with conflict prompts.
There is an operations history for inspection—not a full multi-step undo.

### Odds and ends

- **Read-only mode** (**Ctrl+Shift+R**) blocks accidental writes
- Save the visible file list (**Ctrl+Shift+S**)
- Properties dialog, Open With…, and context menus grouped like a normal
  desktop file manager
- CLI helpers in the same tree: `dt-copy`, `dt-filter`, `dt-mediainfo`, and
  friends (`--help` or man pages after install)

---

## Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| **F2** | Rename |
| **F3** / **Ctrl+F** | Recursive search |
| **Ctrl+T** | Tag selection |
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

With Nix (from this directory):

```bash
nix develop
cmake -B build -G Ninja && cmake --build build
```

`nix build` builds the package without running tests. `nix flake check` runs
the unit tests using the already-built binary when it’s in the store.

---

## Layout (developers)

| Path | Purpose |
|------|---------|
| `apps/dirtoo` | GUI |
| `libs/dirops` | Copy / move / rename / delete / mkdir |
| `libs/dirtoo-fs` | Locations, file info, listing |
| `libs/dirtoo-collection` | Sort, filter, group |
| `libs/dirtoo-filter` | Filter language + media metadata |
| `libs/dirtoo-hash` | Checksums + SQLite cache |
| `libs/dirtoo-tags` | Tag definitions and associations |
| `libs/dirtoo-watcher` | Directory change notifications |
| `libs/dirtoo-thumbnail` | Thumbnailer client |
| `libs/dirtoo-archive` | Read-only archive access |
| `tools/` | `dt-*` CLI helpers |
| `tests/` | Catch2 tests |

Contributor rules and open work: [`../AGENTS.md`](../AGENTS.md),
[`../TODO.md`](../TODO.md).

### Not in scope right now

Writing into archives, remote filesystems (SMB, SFTP, …), and a full undo stack.

---

## License

**GPL-3.0-or-later**. Source files carry REUSE-style SPDX headers.
