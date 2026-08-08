# dirtoo

**dirtoo** is a local file manager for Linux. It is built for people who live in
folders of photos, videos, and mixed media: fast filtering, rich thumbnails, and
keyboard-friendly browsing without giving up ordinary copy/paste and drag-and-drop.

> This application is under active development. Prefer not to rely on it as the
> only tool for irreplaceable data. A one-time warning appears at startup (you
> can dismiss it permanently).

The active implementation is **C++23 / Qt6**. An older Python version lives in
`../dirtoo-py/` as a behavioral reference only.

---

## What you can do

### Browse and navigate

- Open folders from the **location bar** (breadcrumb or editable path).
- **Back / Forward**, **Parent**, and **Home** from the toolbar or shortcuts.
- **Sidebar** with places, bookmarks, and removable devices (mount / unmount / eject when UDisks is available).
- **Bookmarks**, **history**, and **Recently Opened** files.
- **New window** (Ctrl+N). **Middle-click** a folder, breadcrumb segment, or history entry to open it in another window.
- Browse **archives** (zip, tar, tar.gz, 7z, rar, …) read-only by double-clicking them.
- Window title shows the current path (shortened when long); the taskbar/icon name uses a tighter form where the desktop supports it.

### View modes

| Mode | Best for |
|------|----------|
| **Detail** | Columns, sorting, dense lists |
| **Icons** | Thumbnails, media badges, visual browsing |
| **Small icons** | Compact icon grid |

In Icons mode you can **zoom**, switch crop vs letterbox thumbnails, and show
media badges (size, duration, fps, page count, and similar). Folders can show a
**montage** of child thumbnails. Hidden files can use a distinct muted look.
Read- and write-protected files show permission badges.

### Filter (current folder)

The filter bar narrows what you see **in the current directory** without starting
a full-disk search. Results update as you type; **Enter** keeps the filter and
returns focus to the file list.

Examples:

```text
*.png
size:>1M type:file
type:video
contains:hello
date:2024
aspect:16:9
(a or b) and not c
```

Use **Help → Filter expression help** in the app for the full language, or
`dt-filter --help` on the command line. Filter history is available with
**Up / Down** in the filter field.

### Search (recursive)

The search row runs a **recursive** search with the same expression language.
Use it when the file may be nested under the current location.

### Thumbnails

Thumbnails come from the desktop **freedesktop thumbnailer** (D-Bus) when
available, with sensible fallbacks. Directory tiles can request child thumbs and
compose a small grid so video-heavy folders still look representative.

### Select, open, and edit

- Click to select; **Ctrl-click** to add or remove items (standard multi-select).
- In **Icons** mode a keyboard **cursor** (outline) moves with the arrow keys:
  - **Space** — toggle selection on the cursor item
  - **Shift + arrows** — toggle the current item, then “paint” that same on/off
    state onto tiles you move through while Shift is held
  - **Escape** — clear the selection (cursor stays)
- **Enter** opens the item under the cursor (or activates the default action).
- **F2** rename, **F3** properties (with thumbnail preview), **Delete** delete.
- Clipboard **Cut / Copy / Paste**, including **link** paste where supported.
- Drag and drop between windows and onto folder tiles; modifier keys choose
  **copy**, **move**, or **link** where the desktop allows.

### Transfers

Long operations run in a **transfer dialog**: progress, byte counts, pause /
resume, activity log, and conflict handling (replace, rename, skip, apply to all).
Archive members are treated as read-only sources.

### Safety and history

- **Read-only mode** (toolbar / Ctrl+Shift+R) blocks filesystem changes.
- **Operations history** records recent renames, moves, copies, deletes, and
  similar actions (inspection log — not a full undo stack).

### Command-line tools

The same tree ships small `dt-*` utilities (copy, move, rename, mkdir, filter,
mediainfo, archiveinfo, …). See `man/` after install, or run each tool with
`--help`.

---

## Keyboard shortcuts (common)

| Shortcut | Action |
|----------|--------|
| **F2** | Rename |
| **F3** / **Ctrl+F** | Recursive search |
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
| **Escape** | Clear selection (Icons cursor mode) |

Type-ahead: with the file view focused, typing printable characters either jumps
via the leap widget or goes to the filter bar when it is visible.

---

## Build and run

```bash
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build          # optional
cmake --install build           # binaries + man pages
```

With Nix:

```bash
nix develop
cmake -B build -G Ninja && cmake --build build
```

Run the GUI:

```bash
./build/apps/dirtoo/dirtoo
```

---

## Project layout (for developers)

| Path | Purpose |
|------|---------|
| `apps/dirtoo` | GUI application |
| `libs/dirops` | Copy / move / rename / delete / mkdir (Qt-free) |
| `libs/dirtoo-fs` | Locations, file info, listing |
| `libs/dirtoo-collection` | Sort, filter, group |
| `libs/dirtoo-filter` | Filter language + media metadata cache |
| `libs/dirtoo-watcher` | Directory change notifications |
| `libs/dirtoo-thumbnail` | Freedesktop thumbnailer client |
| `libs/dirtoo-archive` | Read-only archive listing and extract-on-demand |
| `tools/` | `dt-*` command-line helpers |
| `tests/` | Catch2 tests |
| `resources/` | Icons, badges, cursors |

Contributor rules and open work: [`../AGENTS.md`](../AGENTS.md), [`../TODO.md`](../TODO.md).

### Intentional limits

Not in scope right now: writing into archives, remote/network filesystems (SMB,
SFTP, …), and a full multi-step undo system.

---

## License

**GPL-3.0-or-later**. Source files use REUSE-style SPDX headers.
