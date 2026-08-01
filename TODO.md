# TODO — dirtoo C++ port (UI parity & remaining work)

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status

Core file manager MVP is **done** (browse, mutate via dirops, DND, clipboard,
archives read-only, multi-window, packaging). See completed checklist below.

---

## Python UI analysis → C++ parity

Findings from `dirtoo-py` GUI / fileview (high-value behaviours):

### Navigation & mouse

| Python behaviour | Source | C++ status |
|------------------|--------|------------|
| Middle-click breadcrumb → new window | `location_buttonbar.py` | **done** |
| Middle-click directory item → new window | `file_item.py` | **done** |
| Middle-click archive → open archive in new window | file item | **done** |
| Middle-click Parent tool button → parent in new window | `tool_button.py` + window | **done** |
| Middle-click History / menu entries → new window | `menu.py` `addDoubleAction` | **todo** |
| Click empty location bar → line edit | buttonbar | **done** |
| Path completion while typing location | `path_completion.py` | **done** (QFileSystemModel completer; not async worker) |
| Location history menu (unique past locations) | `history_menu.py` | **done** (History menu; middle-click on entries TBD) |
| Bookmarks protocol / menu | file_view_window | **todo** / low priority |

### Filter & search

| Python behaviour | Source | C++ status |
|------------------|--------|------------|
| Filter toolbar (show/hide, pin) | `filter_line_edit.py` | **partial** (Ctrl+F show/hide; no pin yet) |
| Filter text history (Up/Down) | `filter_line_edit.py` | **done** |
| Escape clears / hides filter | filter line edit | **done** (clear; second Escape leaves location edit) |
| Substring + glob filter | collection | **done** |
| Full filter language (`filter/`, expr parser) | `filter/*.py` | **todo** (large; defer unless needed) |
| Content / recursive search stream | `search_stream.py` | **todo** / defer |
| Filter help dialog | controller | **todo** |

### View & chrome

| Python behaviour | Source | C++ status |
|------------------|--------|------------|
| Detail + icon views, zoom | file_view / scaler | **done** |
| **Leap widget** (type-to-jump, bottom-right overlay) | `leap_widget.py` | **done** |
| Message area (transient status/errors) | `message_area.py` | **partial** (status bar only) |
| Preferences dialog | `preferences_dialog.py` | **partial** (QSettings without UI) |
| Transfer / conflict dialogs | gui/* | **done** |
| Properties dialog | properties | **done** |
| Drag “edge” / resize grip styling | drag_widget / style | **todo** (low; Qt native grips OK) |
| Graphics-scene icon layout (file_graphics_scene) | fileview | **out of scope** for MVP Widgets port |

### Ops & filesystem

| Python behaviour | C++ status |
|------------------|------------|
| dirops-style copy/move/rename/mkdir/delete | **done** (`dirops`) |
| Archive browse (extract/cache) | **done** (read-only, TOC-first) |
| Virtual FS / multi-protocol stacks | **partial** (file + archive only) |
| Thumbnailer D-Bus | **done** |

---

## Completed MVP checklist

- [x] CMake + flake; VERSION; independent lib packages
- [x] Browse, dual view, zoom, breadcrumbs, location completer
- [x] dirops mutations, transfers, conflicts, DND, clipboard
- [x] Watcher, thumbnails, archives read-only
- [x] Multi-window (Ctrl+N, middle-click breadcrumb & directory)
- [x] Filter glob + history (Up/Down)
- [x] About, desktop, AppStream, settings

---

## Near-term UI queue (from analysis)

1. [x] **Leap widget** — frameless type-ahead jump overlay (Python `LeapWidget`)
2. [x] **Location history menu** — unique past paths, middle-click opens new window
3. [x] **Middle-click on toolbar Parent** (and other nav buttons) → new window
4. [x] Filter show/hide (pin still open) (bottom toolbar like Python)
5. [ ] Preferences dialog surface for QSettings keys
6. [ ] Optional: async path completion worker (large dirs)
7. [ ] Optional: simplified subset of filter language (`size>`, `mtime:`)

---

## Explicitly defer / ignore

| Item | Reason |
|------|--------|
| Write into archives | Policy: read-only |
| Graphics View icon scene | Different architecture |
| Face detect / experiments / most programs/* | Out of scope |
| Pixel-perfect Python layout | Functional parity only |
| Full expr/filter DSL | Large; glob+substring first |

---

## Working process

- Suggest a detailed commit message after each series of changes.
- Keep `dirtoo-py/` as reference only.
