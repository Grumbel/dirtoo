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
| Media metadata predicates | **done** (ffprobe: width/height/duration/framerate) |

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


---

## Responsive UI & metadata architecture (in progress)

**Guiding rule:** the GUI thread must not perform filesystem or network I/O.
That keeps the UI fluid today and leaves room for virtual backends later
(archive views, SFTP, other VFS). Listing, probing, thumbnails, and cache
reads/writes belong on workers; the UI only applies results and paints from
in-memory state.

Python used per-file XML metadata sidecars — too slow at scale. C++ will use a
**single SQLite database** for durable media (and later general) metadata.

### SQLite metadata cache

| Topic | Decision |
|-------|----------|
| Store | `$XDG_CACHE_HOME/dirtoo/meta.sqlite` (fallback `~/.cache/dirtoo/`) |
| Key | Absolute path + `mtime_ns` + `size` (invalidate when either changes) |
| Columns (v1) | path, mtime_ns, size, width, height, duration_ms, framerate, probed_at |
| Later | mime/type, pages, file_count, checksum hooks, backend id for non-file URIs |
| Writers | Worker threads only; one connection per worker or serialized write queue |
| Readers | Workers hydrate an **in-memory** map; GUI reads memory only |
| Probe | ffprobe (override `DIRTOO_FFPROBE`); skip non-media extensions |
| CLI | `dt-filter` / tools may use the same cache on their own threads |

Schema evolution via `PRAGMA user_version` migrations.

### Async layers (ordered)

1. **MediaMetaCache (memory + SQLite + worker pool)** — done/in progress  
   - `try_get(path)` → optional in-memory hit (no I/O)  
   - `request(path, gen)` → enqueue; worker: SQLite → maybe ffprobe → memory → notify  
   - Paint/sort/filter **never** call ffprobe or SQLite on the GUI thread  

2. **FileItemDelegate** — paint from `try_get` only; request missing rows for viewport  

3. **DirectoryLoadWorker** — `list_directory` / archive TOC off UI thread; generation token  

4. **Sort worker** — name/size/mtime off-thread; media sorts wait on meta coverage or use cached fields only  

5. **Filter** — cheap predicates immediate; media predicates match against memory cache / worker  

6. **Thumbnails** — keep D-Bus async; debounce + viewport priority; shared concurrency budget with meta pool  

7. **VFS readiness** — `Location` already abstracts path vs archive; future SFTP provider implements the same async list/stat/read APIs with no GUI I/O  

### Viewport & generation

- Only enqueue meta/thumbnails for **visible** rows (+ small margin).  
- Bump `generation` on navigate / filter / sort change; drop stale callbacks.  
- Bound concurrency (e.g. 2–4 ffprobe jobs).

### Success criteria

- Large directories: chrome stays responsive while listing fills in.  
- Icon scroll over videos: no stalls; badges appear progressively.  
- Revisit folder: meta loads from SQLite without re-ffprobe when mtime/size match.  
- No `stat` / `popen` / SQLite from paint or other GUI-thread slots.

### Implementation checklist

- [x] SQLite schema + open/migrate helpers  
- [x] Disk cache get/put keyed by path+mtime+size  
- [x] In-memory layer + worker queue (`request` / `try_get`)  
- [x] Wire FileItemDelegate to memory-only + viewport requests (paint path)  
- [ ] Stop sync `probe_media` from sorter/filter on UI paths (use cache fields)  
- [ ] Async directory load  
- [ ] Async sort  
- [ ] Debounced thumbnails  
- [ ] Optional: persist more FileInfo fields; multi-backend location keys  


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
- Still open vs Python: `contains:`. Media width/height/duration/framerate via ffprobe; fuzzy n-gram done.

---

## Working process

- Suggest a detailed commit message after each change series.
- Keep `dirtoo-py/` as reference only.
