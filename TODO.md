# TODO — Porting dirtoo from Python/PyQt6 to C++23

This document is the working plan for the C++ port. The Python tree in
`dirtoo-py/` remains the behavioral reference. The active codebase is
`dirtoo/`.

## Progress (2026-08-01)

MVP + archive browsing foundation:

- Full GUI file manager workflows (views, DND, clipboard, transfers, menus, settings)
- Archive locations (`archive:///path.zip!/entry`) via ArchiveManager
- Cache extract using bsdtar/tar/unzip/7z; read-only browsing inside archives
- Unit tests for location/archive URL, collection, clipboard, dirops

Still optional:
- Write support into archives
- Nested archives
- libarchive direct integration instead of external tools

Principles (see also `AGENTS.md`):

- Functional modular file manager; keep general UI and capabilities, not
  every quirk or unfinished experiment.
- **dirops** (copy/move/rename/delete/…) is a separate library from day one.
- Clean design; no hacks; debug logs when stuck.
- C++23, CMake, Nix flake, GPLv3+ SPDX headers.
- Most `dirtoo-py/src/dirtoo/programs/*` utilities are **low priority or
  ignored**; only port those that help test C++ libraries.

---

## Phase 0 — Repository & build skeleton

**Status: first work item**

- [ ] Create `dirtoo/` top-level tree:
  ```
  dirtoo/
    CMakeLists.txt
    flake.nix
    flake.lock          # generate via nix
    README.md
    LICENSE             # or rely on SPDX + COPYING
    libs/
      dirops/
      dirtoo-fs/
      dirtoo-collection/
      dirtoo-watcher/
      dirtoo-thumbnail/
    apps/dirtoo/
    tools/
    tests/
    resources/
  ```
- [ ] Root `CMakeLists.txt`: C++23, `CMAKE_CXX_STANDARD 23`, warnings
  (`-Wall -Wextra -Wpedantic`), optional sanitizers.
- [ ] `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)` (add
  DBus later).
- [ ] `flake.nix`: `qt6`, `cmake`, `ninja`, `pkg-config`, `gtest` or
  Catch2, `inotify-tools` headers if needed; `devShell` for local work.
- [ ] Install targets for libraries and the `dirtoo` binary.
- [ ] CI-friendly: `cmake -B build && cmake --build build && ctest`.

**Suggested commit:** `Scaffold C++ project with CMake, flake, and empty libs`

---

## Phase 1 — Core filesystem model (`dirtoo-fs`)

Mirror the useful parts of `filesystem/location.py` and
`filesystem/file_info.py` without archive payload complexity at first.

### 1.1 Location

- [ ] Class `dirtoo::fs::Location`
  - Protocol + absolute path (start with `file://` only).
  - `from_path`, `from_url`, `from_human`, `as_url`, `as_path`.
  - `parent()`, `join()`, `basename()`, `dirname()`.
  - Equality / hash for use in maps.
  - **Defer** multi-payload archive URLs (`file.rar//rar:…`) to Phase 6.
- [ ] Unit tests: path edge cases, parent of root, join, URL round-trip.

### 1.2 FileInfo

- [ ] Class `dirtoo::fs::FileInfo`
  - From `std::filesystem` / `stat`/`lstat`: size, mtime, mode, uid/gid,
    is_dir / is_file / is_symlink, symlink target.
  - Basename, extension, display name.
  - Optional lazy MIME (via QMimeDatabase or xdgmime later).
- [ ] `StdioFilesystem` (or free functions) to list directory →
  `vector<FileInfo>`.

### 1.3 Errors

- [ ] `dirtoo::fs::Error` / `std::expected`-friendly error type
  (errno + message + path).

**Suggested commit:** `Add dirtoo-fs Location and FileInfo with unit tests`

---

## Phase 2 — dirops library (extractable later)

Port the spirit of `posix/filesystem.py` and
`fileview/filesystem_operations.py` as a **Qt-free** library.

### 2.1 API sketch

```cpp
namespace dirops {

enum class ConflictPolicy { Fail, Overwrite, Rename, Skip };
enum class OpKind { Copy, Move, Rename, Remove, Mkdir, Touch };

struct Options {
  bool dry_run = false;
  bool verbose = false;
  ConflictPolicy conflict = ConflictPolicy::Fail;
  // progress callback: bytes done / total, current path
};

struct Result {
  // per-item outcomes, cancelled flag, etc.
};

std::expected<Result, Error> copy(const std::filesystem::path& from,
                                  const std::filesystem::path& to,
                                  const Options& = {});
std::expected<Result, Error> move(...);
std::expected<Result, Error> rename(...);
std::expected<Result, Error> remove(...);      // file or recursive dir
std::expected<Result, Error> mkdir(...);
std::expected<Result, Error> swap_names(...);  // like dt-swap

}
```

### 2.2 Implementation tasks

- [ ] Same-filesystem rename vs cross-device copy+delete for move.
- [ ] Directory merge policy (Python `dt-move` merges into existing
  dirs — decide explicitly; document; implement one clear behavior).
- [ ] Progress and cancellation hooks (for GUI transfer dialog later).
- [ ] No GUI includes in this library.
- [ ] Unit tests with temporary directories (success, EACCES, conflict).

### 2.3 Test CLI tools (only ones worth porting early)

- [ ] `tools/dt-copy` — thin CLI over `dirops::copy`
- [ ] `tools/dt-move` — thin CLI over `dirops::move`
- [ ] `tools/dt-swap` — thin CLI over `dirops::swap_names`

**Ignore for now:** dt-find, dt-fsck, dt-fuzzy, dt-glob, dt-expr,
dt-archiveinfo, dt-guessarchivename, dt-mediainfo, dt-metadata,
dt-thumbnailer, dt-unidecode, dt-chomp, dt-shuffle, dt-rmdir,
dt-mkevil, dt-mktest, dt-sleep, dt-desktop, dt-icon, dt-mime, etc.

**Suggested commit:** `Introduce dirops library with copy/move/rename and CLI tools`

---

## Phase 3 — Directory watching (`dirtoo-watcher`)

- [ ] inotify wrapper (or Qt `QFileSystemWatcher` first for simplicity;
  upgrade to inotify for CREATE/DELETE/MODIFY granularity matching
  Python).
- [ ] Emit typed events: added / removed / modified / closed /
  scandir_finished.
- [ ] Thread or Qt event integration cleanly (no blocking UI).
- [ ] Tests with a temp dir and synthetic create/delete.

**Suggested commit:** `Add dirtoo-watcher with inotify-backed directory events`

---

## Phase 4 — Collection, filter, sort (`dirtoo-collection`)

- [ ] `FileCollection`: ordered list + key lookup (by Location).
- [ ] Sorter: name (numeric-aware), size, mtime, type; ascending/desc.
- [ ] Filter: simple name glob / substring first; full Python filter
  language later if needed.
- [ ] Grouper: optional (directory / day) — lower priority.

**Suggested commit:** `Add FileCollection with sort and basic name filter`

---

## Phase 5 — Minimal GUI application

Goal: open a directory, see files, navigate, basic operations via dirops.

### 5.1 Application shell

- [ ] `QApplication` entry (`apps/dirtoo/main.cpp`).
- [ ] `MainWindow`: toolbar, location line edit or breadcrumb bar,
  central view, status/message area.
- [ ] Controller object coordinating location, collection, view
  (MVC-ish; keep logic out of widgets).

### 5.2 Views

- [ ] Start with `QListView` / `QTreeView` + custom model for **detail**
  and **icon** modes (faster to ship than full `QGraphicsScene` port).
- [ ] Later optional graphics scene layout if needed for fancy tile
  layouts / timespace (Python experiments).
- [ ] Zoom / icon size levels.
- [ ] Selection model; keyboard navigation.

### 5.3 Navigation

- [ ] Open location, parent, home, history back/forward.
- [ ] Path completion for location bar.
- [ ] React to watcher events (refresh or incremental update).

### 5.4 File operations (UI → dirops)

- [ ] Rename dialog → `dirops::rename`
- [ ] Create folder/file dialogs → `dirops::mkdir` / touch
- [ ] Delete with confirmation → `dirops::remove`
- [ ] Copy/cut/paste using clipboard + dirops (GNOME
  `x-special/gnome-copied-files` optional later)
- [ ] Conflict dialog wired to `ConflictPolicy`
- [ ] Transfer progress dialog using dirops progress callbacks

### 5.5 Context menus & actions

- [ ] File actions: open (xdg-open), open with, properties, rename,
  delete, copy path.
- [ ] Directory actions: open in terminal (optional), new folder.
- [ ] View actions: icon/detail, sort order, show hidden.

### 5.6 Dialogs (port as needed)

Priority: rename, create, conflict, properties, about.  
Defer: preferences polish, transfer error variants until ops are solid.

**Suggested commits (split):**
- `Add MainWindow shell with location bar and directory listing`
- `Wire navigation history and directory watcher to the view`
- `Connect rename/delete/mkdir to dirops with basic dialogs`

---

## Phase 6 — Thumbnails & MIME

- [ ] D-Bus client for org.freedesktop.thumbnails.Thumbnailer1
  (`dirtoo-thumbnail`).
- [ ] Cache path helpers (XDG thumbnail spec).
- [ ] Request thumbnails for visible items; placeholder icons otherwise.
- [ ] MIME icons via `QMimeDatabase` / freedesktop icon theme.
- [ ] Directory “composite” thumbnails — optional, later.

**Suggested commit:** `Add freedesktop thumbnailer client and icon view thumbnails`

---

## Phase 7 — Archives (optional / later)

- [ ] Abstract extractor interface.
- [ ] libarchive and/or 7z subprocess; RAR only if nonfree allowed.
- [ ] Extend `Location` with payload stack for browsing inside archives.
- [ ] Cache extracted trees under XDG cache (like Python).

Until then, archives appear as normal files (open with external app).

---

## Phase 8 — Filter language & search (optional)

- [ ] Simple filter line (name contains / glob) is enough for MVP.
- [ ] Full expression language (`filter/`, `expr/`) only if users need it.
- [ ] Content search / recursive find: low priority CLI or later UI.

---

## Phase 9 — Polish & packaging

- [ ] Desktop file + icons under `resources/`.
- [ ] Settings (QSettings): window geometry, view mode, sort.
- [ ] About dialog, license text.
- [ ] Nix package output; optional system install via CMake.
- [ ] README with build instructions.
- [ ] Audit: no remaining Python-only assumptions in C++ code.

---

## Explicitly out of scope / ignore

| Item | Reason |
|------|--------|
| `experiments/` | Prototypes and crash investigations |
| Most `programs/*` CLI tools | Not needed for GUI MVP; dirops CLIs are enough |
| Face detection, QML experiment, UDisks demos | Unrelated experiments |
| Pixel-perfect matching of Python layouts | Clean, usable UI is the goal |
| Cloning known Python bugs / incomplete features | Fix by design instead |
| Windows/macOS support in first milestones | Linux-first; abstract later |

---

## Dependency map (C++)

| Need | Library / component |
|------|---------------------|
| UI | Qt6 Widgets, Gui, Core |
| Thumbnail D-Bus | Qt6 DBus |
| FS paths | `std::filesystem` |
| Tests | Catch2 or GoogleTest |
| Build | CMake ≥ 3.25, Ninja |
| Dev env | Nix flake |
| Archives (later) | libarchive, optional 7z |
| Media metadata (later) | optional mediainfo / ffmpeg |

Avoid pulling numpy/scipy-style dependencies; the Python image filters
are not required for MVP.

---

## Milestone checklist (MVP file manager)

When these are done, the C++ app is a usable MVP:

1. Build with CMake + flake.
2. Open home / arbitrary directory; list files (icon + detail).
3. Navigate parent/home/history; location bar works.
4. Rename, mkdir, delete via dirops.
5. Copy/move with conflict handling and basic progress.
6. Directory auto-refresh via watcher.
7. Thumbnails or at least MIME icons.
8. Context menu open with default application.

Everything else is iterative improvement.

---

## Working process reminders

- After each coherent change series, output a **detailed suggested git
  commit message**.
- Prefer small PRs/commits: library → tests → UI wiring.
- If blocked, add debug logging and document the hypothesis in the
  commit message or a short note in this file under a “Blockers”
  section (add when needed).
- Keep `dirtoo-py/` intact as reference; do not “fix” it as part of
  the port.
