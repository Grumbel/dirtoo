# TODO — dirtoo C++ port (UI parity & remaining work)

Python reference: `dirtoo-py/`. Active code: `dirtoo/`.

---

## MVP status

Core file manager MVP is **done**. Filter DSL, multi-window, dirops, archives
(read-only), and packaging are in place.

---

## Python UI analysis → C++ parity

### Navigation & mouse

| Python behaviour | C++ status |
|------------------|------------|
| Middle-click breadcrumb / directory / archive → new window | **done** |
| Middle-click Parent toolbar | **done** |
| Middle-click History menu entries → new window | **done** (`HistoryMenu`) |
| Location bar ↔ line edit, path completer | **done** |
| Location history menu | **done** |
| Bookmarks menu (file store, middle-click) | **done** |

### Filter & search

| Python behaviour | C++ status |
|------------------|------------|
| Filter show/hide + pin, history Up/Down, Escape | **done** |
| Filter DSL (`and`/`or`/`not`/`()`, glob, regex, size, type) | **done** (`dirtoo-filter`) |
| Filter help | **done** (Help → Filter expression help) |
| CLI filter tool | **done** (`dt-filter`) |
| Content / recursive search | partial: recursive name/filter search done; content `contains:` still open |
| Media metadata predicates | **todo** (optional deps) |

### View & chrome

| Python behaviour | C++ status |
|------------------|------------|
| Detail + icons, zoom, leap widget | **done** |
| Message area | **done** (transient banner) |
| Preferences, transfers, properties, About | **done** |
| Async path completion worker | **done** |
| Graphics View icon scene | deferred (may revisit for flexibility) |

### Ops

| Item | Status |
|------|--------|
| dirops + tools | **done** |
| Archives read-only | **done** |
| Thumbnails D-Bus | **done** |

---

## Near-term queue

1. [x] Leap widget
2. [x] History menu + middle-click new window
3. [x] Middle-click Parent
4. [x] Filter show/hide + pin
5. [x] Preferences dialog
6. [x] Optional: async path completion worker
7. [x] Filter DSL + `()` grouping
8. [x] `dt-filter` CLI
9. [x] Message area
10. [x] Bookmarks menu
11. [ ] Optional: `contains:` content match (careful with large files)
12. [x] Optional: recursive directory search UI/CLI (`dt-filter -r`, View → Recursive Search)

---

## Explicitly defer / ignore

| Item | Reason |
|------|--------|
| Write into archives | Read-only by design |
| Graphics View icon scene | May revisit; Model/View for now |
| Face detect / experiments / most programs/* | Out of scope |
| Pixel-perfect Python layout | Functional parity only |
| Media metadata filters | Optional backends later |

---

## Filter DSL notes (`dirtoo-filter`)

- Parentheses grouping fixed vs Python.
- Modular Qt-free library; `dt-filter '<expr>' [dir]` for CLI testing.
- Still open vs Python: `contains:`, fuzzy, width/height/duration.

---

## Working process

- Suggest a detailed commit message after each change series.
- Keep `dirtoo-py/` as reference only.
