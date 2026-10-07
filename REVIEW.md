# Code review — open issues (2026-10-07)

Findings from a read-through of the build system, the libraries and the parts
of the GUI that touch the filesystem. Everything *fixed* during the review is
listed at the bottom for context; the sections above it are the **open**
issues, to be tackled later.

Conventions:

- **Sev**: `H` can lose data / crash / is a security issue, `M` wrong results or
  a clear UX/perf problem, `L` polish or cleanup.
- **Verified** says how sure the finding is: *repro* = reproduced with a
  test/run, *read* = found by reading the code only (not run), *unverified* =
  suspected, needs a look before acting.
- IDs are stable; reference them in commits (`Refs REVIEW A2`).

---

## A. Build system, packaging, CI

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| A1 | L | read | Every `libs/*/CMakeLists.txt` repeats ~40 lines (version parsing, standard, install, export, config files). A shared `cmake/DirtooLib.cmake` would remove the drift risk, **but** the flake gives each lib a scoped `lib.fileset`, so the module has to be added to every lib's fileset (or each lib stays self-contained). Decide which. |
| A2 | M | read | No CI at all (`.github/` missing). Minimum useful job: `nix flake check` (builds `.#dirtoo`, runs the tests). Packaging breakage (propagated deps, config files) is currently only noticed by hand. |
| A3 | L | read | Warning flags are set per library, privately and inconsistently; the app/tests use `dirtoo_warnings`. No `-Werror` option for CI, no sanitizer (ASan/UBSan/TSan) option, `clang-tidy`/`clang-format` are in the dev shell but have no config. The use-after-free in the watcher and the stack overflows in the filter would have been caught by an ASan/TSan test run. |
| A4 | L | read | `apps/dirtoo/CMakeLists.txt` is a hand-maintained list of ~150 sources and ~60 installed icon files. Consider `install(DIRECTORY resources/icons …)` and `target_sources` per feature group. |
| A5 | L | read | `flake.nix` is ~550 lines; the dev-shell scripts (`devPreamble`, `devScripts`) could move to `nix/dev.nix`. |
| A6 | L | read | `AUDIT.md` still describes the old `bsdtar`/`tar`/`unzip` archive implementation in several places (≈ lines 864, 1027, 1495, 1504, 1961). Refresh or mark as historical. |
| A7 | M | read | `media_probe.cpp` shells out through `popen("… 2>/dev/null")` with hand-rolled quoting (`quote_path`) for `ffprobe` and `pdfinfo`. Use `posix_spawn`/`QProcess` with an argv vector (no shell), and add a timeout: a hung `ffprobe` on a stalled mount blocks a worker thread indefinitely. |
| A8 | L | read | `DIRTOO_BUILD_TESTS` now requires `DIRTOO_BUILD_APP` (tests link `dirtoo-app-core`). If tests of the libraries alone are wanted, move more shared code out of `apps/`. |

## B. dirops and transfers

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| B1 | M | read | `unique_path()` + create is check-then-act; another process can take the name in between. Fix by creating with `O_EXCL`/`mkdir` and retrying with the next suffix inside dirops instead of probing first. |
| B2 | L | read | `swap_names` cannot roll back if the final rename fails; the error names the temp path that holds the original. A journal or retry would be better. |
| B3 | H | read | **Delete is permanent** (`remove_all` after a confirmation dialog). No trash (freedesktop `trash-spec`), no undo. Probably the biggest safety gap for a file manager. Needs a design decision (trash vs. permanent as separate commands). |
| B4 | M | read | `TransferWorker` stops at the first failed item; the remaining items are neither tried nor reported as untouched. Options: continue and collect errors, or ask (Skip/Retry/Abort). |
| B5 | L | read | Paste-as-Link in the folder of the source fails with "already exists" instead of picking a free name (`x (2)` / Python's "Link to x"). Use `ConflictPolicy::Rename` or ask. |
| B6 | M | read | `TransferController::shutdown()` waits 5 s for the worker thread; if a copy is blocked on a slow drive the `QThread` is then destroyed while running (Qt aborts). Detach like `ThumbnailCoordinator::stop_thread` does. |
| B7 | L | read | `copy_regular_file` streams with `ofstream` in 256 KiB chunks: no `copy_file_range`/reflink, no sparse-file handling, ownership/xattrs not preserved, no fsync. Fine for now; matters for large media copies. |
| B8 | L | read | Cross-device `move_path` of a directory is "copy then `remove_all`": a source file modified during the copy is lost, and symlinks/special files are copied/refused rather than moved. Acceptable, but worth documenting in `ops.hpp`. |

## C. Archives

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| C1 | M | read | Opening an archive **extracts all of it** into `~/.cache/dirtoo/archives` (multi-GB archives, slow, disk use). A member-level browse (listing + `extract_member_libarchive` already exist) plus lazy extraction would avoid it. |
| C2 | M | read | The archive cache is never evicted (only `archive_member_cache` has `max_age`). Needs size/age eviction. |
| C3 | L | read | `ARCHIVE_EXTRACT_PERM` keeps archive directory modes; a directory stored as `000` becomes un-browsable and `remove_all` of the cache dir may fail. Mask dirs with at least `u+rwx` after extraction. |
| C4 | L | read | `extract_member_libarchive` does not pass `ARCHIVE_EXTRACT_SECURE_SYMLINKS` (full extraction does). Low risk (single entry) but inconsistent. Note: libarchive's symlink check also looks at the destination prefix; verify it is happy with a symlinked `~/.cache` before adding it. |
| C5 | L | read | Archive entries with absolute or `..` names are **skipped silently** on extraction (and listed as normal entries in `list_archive_entries`). Surface a warning to the user. |

## D. dirtoo-fs / FileInfo

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| D1 | M | read | `FileInfo` keeps whole-second mtimes (`sys_from_unix(tv_sec)`), dropping nanoseconds. Files created within the same second sort unstably by time and "newer than" comparisons are coarse. Keep `tv_nsec`. |
| D2 | L | read | `set_mtime_unix(sec <= 0)` is ignored, so a real mtime of exactly 0 (or before 1970) is shown as "unknown". Use an explicit `has_mtime` flag. |
| D3 | L | read | `FileInfo::from_path`/`from_directory_entry` use `try/catch (...)` around name conversion: silent catches contradict the "log, don't paper over" rule. |

## E. dirtoo-hash / dirtoo-tags (SQLite stores)

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| E1 | H | read | **Tag lookups ignore staleness.** `TagStore::tags_for_path` and `resolve_path` use `ChecksumStore::get()` without checking size/mtime, so after a file is edited it keeps the tags of its *old* content, and tagging a changed file attaches the tag to the old hash. Needs `get_if_valid` with the file's current size/mtime, which must be supplied by the caller (no stat on the GUI thread) — see E2 for the unit mismatch. |
| E2 | M | read | The stored `mtime_ns` is `file_time_type::time_since_epoch().count()`: libstdc++'s file clock has a non-Unix epoch (I believe 2174, not verified) and an unspecified unit. The filter/collection use whole Unix seconds, so the two cannot be compared. Store Unix nanoseconds (`clock_cast<system_clock>`); this invalidates the existing cache once. Touches `checksum_store.cpp`, `hash_file.cpp`, `apps/dirtoo/hash_service.cpp`. |
| E3 | M | read | `ChecksumStore::put/remove` ignore SQLite errors (e.g. `SQLITE_BUSY` after the 5 s timeout). A lost write looks like success. Return `bool`/`std::expected` and log. |
| E4 | L | read | Nothing prunes the stores: moved/deleted files stay in `checksums` and `paths`, so `paths_for_hash` can return paths that no longer exist (ghost duplicates), and `files` rows without tags accumulate. Add a vacuum/prune command (and a GUI action or periodic job). |
| E5 | L | read | `get_if_valid` accepts an entry when the *current* mtime is unknown (size match only). |
| E6 | L | read | The databases are created with the process umask (typically 0644); they list a user's file inventory and tags. Create with 0600. |
| E7 | L | read | Several store functions still ignore `sqlite3_exec` results for PRAGMAs (`journal_mode=WAL` can fail on network filesystems → silently falls back to rollback journal; `foreign_keys=ON` failing would disable cascades). Check and log. |

## F. dirtoo-filter

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| F1 | M | read | Catastrophic-backtracking patterns (`(a+)+b`) still hang a worker thread for minutes; `std::regex` has no match limit/timeout. The proper fix is a different engine (RE2, or PCRE2 with match limits) as a new flake dependency, or running content matching with a deadline. |
| F2 | L | read | Content-regex windows (4 KiB, 1 KiB overlap): a match longer than the overlap that straddles a window boundary can be missed. Documented in the commit, not in the user help. |
| F3 | L | repro | Unquoted `(` ends a command argument (`cre:(a|b)*c` → "invalid argument for 'cre': ''"). The error should hint at quoting (`cre:"(a|b)*c"`). |
| F4 | L | read | `filter_help_text()` / `filter_help_html()` do not mention quoting rules or that bad arguments are now errors. |
| F5 | M | read | Content predicates (`contains*`) re-read up to 1 MiB of every file on every filter change; there is no per-file result cache keyed by (path, mtime, size, expression). Typing in the filter box on a big directory re-reads everything. |
| F6 | L | read | `lookup_media()` in `predicates_detail.hpp` falls back to `resolve_media_cached` synchronously ("CLI / non-GUI"). Make sure no GUI call path reaches it (the `AGENTS.md` GUI-I/O rule). |

## G. dirtoo-collection (sort / group / collection)

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| G1 | M | read | `Sorter::compare` rebuilds `numeric_sort_key(to_lower(basename))` (vector + strings) **twice per comparison**: O(n log n) allocations. Precompute keys once per sort (decorate–sort–undecorate); large directories sort noticeably slower than necessary. |
| G2 | L | read | `numeric_sort_key` overflows `uint64` for digit runs longer than 19 digits and wraps silently → wrong order. Compare long runs as strings (strip leading zeros, compare length then text). |
| G3 | L | read | Name sorting lower-cases ASCII only and compares raw bytes: non-ASCII names sort by UTF-8 byte order (`Ärger` after `zebra`). Use `QCollator` (ICU) or `strxfrm`-style keys. |
| G4 | L | read | `SortKey::Type` is identical to `SortKey::Extension`. Either differentiate (MIME category) or drop. |
| G5 | M | unverified | Media sort keys (`Width`, `Duration`, …) read the memory cache only; items without metadata sort as 0. Check that the view re-sorts when `MediaMetaCache` fills in (`notify_row_changed` only regroups for Duration). |
| G6 | L | read | `SortKey::Random` reshuffles on every `apply_sort`/rebuild, so any watcher refresh or filter change reorders the whole view. Keep a stable per-session seed. |
| G7 | M | read | `FileCollection` keeps a full `FileInfo` copy in both `items_` and `visible_` (paths, strings, symlink targets) and re-copies on every `rebuild_visible`. Store indices into `items_`. `index_of`, `remove`, `group_label_for` are linear scans (`group_label_for` is O(n) per call: O(n²) if used per row). |
| G8 | M | read | `rebuild_visible` evaluates the filter on the calling (GUI) thread when `set_name_filter`/`set_match_func` is used directly; the worker path (`FilterWorker`) avoids it. Make the direct path private or assert. |
| G9 | L | read | `merge_items` appends new entries in `unordered_map` iteration order, so order of ties after a merge is nondeterministic until the next sort. |

## H. GUI: models, views, thumbnails

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| H1 | M | read | `FileListModel::emit_path_changed` scans **all** visible rows and builds a `QString` per row to find the path — called for every thumbnail result/pending/failed. 50 000 files ⇒ O(n²) string allocations. Keep a `path → row` index (rebuilt on layout change). |
| H2 | M | read | `GraphicsFileView::drawForeground` iterates **every** slot and queries `IsGroupStartRole` through the model on every paint (each scroll step). Cache group-start slots, or binary-search the visible range. |
| H3 | M | read | `on_scene_selection_changed` walks all `items_` on every selection change; `compute_layout_slots` is O(n) on every insert/reset; `rowsRemoved` triggers a full `rebuild_items`. This is the "no list virtualization" residual in `AGENTS.md`. |
| H4 | M | read | `FileListModel::refresh()` emits `layoutAboutToBeChanged/layoutChanged` after reordering rows without updating persistent indexes, so `QItemSelectionModel` selection/current index follow *row numbers*, not items, after a sort/filter. The app compensates with `restore_selection_by_paths`; verify every path that reorders goes through it. |
| H5 | L | read | `FileListModel::data()` converts `path()` to `QString` on most calls and looks up several `QHash<QString,…>` maps; `ContentsSize` takes the tree-cache lock and a snapshot per cell paint; `request_child_count` is triggered from the `const` `data()` (via `const_cast`). Cache per-row display strings / use `QString` paths in the model. |
| H6 | L | read | `ThumbnailCoordinator::in_flight_` accounting depends on "every request ends in exactly one ready/failed signal"; `cancel_all()` zeroes it while late signals still arrive (guarded by `> 0`). A per-request id would be more robust. |
| H7 | L | read | `DirectoryLoadWorker::cancel()` invalidates the generation with `cur ^ ~0`, which could in theory collide with a later real generation. Use a separate "cancelled generation" atomic. |
| H8 | L | read | `Cut` leaves the clipboard (and the sources) untouched when a transfer ends partially; the next paste may fail on already-moved items. Track per-item state in the clipboard payload. |

## I. Tests

| ID | Sev | Verified | Issue |
|----|-----|----------|-------|
| I1 | M | read | No tests for the GUI layer: `TransferWorker` (pause/cancel/conflict flow), `MainWindow` ops, `FileListModel`, `GraphicsFileView`. At least `TransferWorker` (it is a plain `QObject`) can be tested headless; the pause/resume race fixed in this review has no regression test. |
| I2 | L | read | Watcher tests wait on real inotify with polling (`pump_until`, 3 s). Fine locally; may flake on loaded CI. Consider a longer timeout or `QSignalSpy`. |
| I3 | L | read | Cross-device `dirops` tests skip when `/dev/shm` is on the same filesystem as `$TMPDIR`; CI should guarantee a second filesystem (or the tests give a false sense of coverage). |
| I4 | L | read | `count_archive_files` (libarchive file counting in `media_probe.cpp`) was only checked manually with `dt-mediainfo`; add a unit test with a generated tar/zip. |
| I5 | L | read | No tests for `Sorter` ordering edge cases (natural sort, ties, descending with directories first) or `FileCollection::merge_items`. |

## J. Not reviewed yet

`tools/` (dt-* CLIs), `thumbnailers/`, `libs/dirtoo-thumbnail` (D-Bus client),
`libs/dirtoo-tree` (only its test was touched), `man/`, `resources/`, and in
`apps/dirtoo`: preferences, properties, bookmarks/history stores, devices/UDisks,
location bar and completion, QuickFilter, Tag Manager, the DnD code
(`graphics_file_view_dnd.cpp`, drag-from-archive extraction), sidebar, and
`main_window_nav/_load.cpp` (only skimmed).

---

## Fixed during this review (for context)

| Commit | What |
|--------|------|
| `c7ee17ce` | flake: dropped unneeded `unzip`/`gnutar`/`p7zip`; added `poppler-utils` for `pdfinfo` |
| `76e10a29` | build: `dirtoo-app-core` shared by app and tests; no baked-in icon source path; Catch2 required |
| `4ed57a7f` | build: alias loop, `DIRTOO_BUILD_TOOLS` default ON, `.gitignore` |
| `397c04a7` | test: wrong expected area in `layout_squarified` test |
| `7a953d67` | filter/archive: archive file count via libarchive (no `bsdtar` popen); dead CLI parsers removed |
| `07371fab` | dirops: cancelled cross-device move deleted the source; move onto itself with Overwrite; dangling symlinks; symlink-to-dir deep copy; unreadable dir copied as empty; copy/move into itself; special files; truncated output on failure |
| `908ef03a` | dirops: copy onto itself with Overwrite destroyed the file |
| `562a6b4c` | app: transfer pause/cancel lost-wakeup hang; dangling-symlink conflicts; Cut in place; missing destination; clipboard kept after error; New Folder "Replace" tree wipe; `.`/`..` names |
| `658a6a56` | archive: absolute/`..` entry names escaped the extract dir; hardlink targets; `ARCHIVE_WARN` treated as fatal; non-atomic shared cache extraction |
| `00080e2a` | load: unreadable/vanished directory shown as empty instead of an error |
| `6da024a2` | watcher: use-after-free on destroy; `IN_Q_OVERFLOW`/`IN_UNMOUNT` ignored; update starvation and unbounded pending lists |
| `7b440315` | filter: `containsre` stack overflow (segfault) on large files |
| `256d890a` | filter: bad arguments / unknown commands are parse errors; nesting depth limit |
| `d3fc3346` | tags: concurrent first-use UNIQUE races; non-atomic set membership move |
