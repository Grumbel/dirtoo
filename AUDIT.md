# AUDIT — dirtoo C++ port vs dirtoo-py

Living document for a **top-to-bottom source audit**.

## Process

1. **Inventory** (this section) — list every tracked source/asset file.
2. **Per-file review** — walk the inventory one file at a time; note:
   - Role / features implemented
   - Bugs, smells, incomplete bits
   - Python parity references (`dirtoo-py/…`)
3. **Parity matrix** — after both trees are reviewed, compare feature
   sets and list C++ gaps vs intentional out-of-scope items.

**Pass status:** C++ inventory rows are marked reviewed. **Per-file notes**
(pass 1) cover libs + GUI overview. **Deep per-file review (pass 2)** adds
call-flow, GUI-thread risk, and concrete defects for apps, workers, and
libraries. Python inventory is a reference map, not a port checklist.

Status legend for file rows:

| Mark | Meaning |
|------|---------|
| ⬜ | Not reviewed yet |
| 🔍 | In progress |
| ✅ | Reviewed — notes below or in § Per-file notes |
| ➖ | Skipped (binary / generated / not source) |

---

## Summary counts

| Tree | Files |
|------|------:|
| `dirtoo/` (C++) | 171 |
| `dirtoo-py/` (Python reference) | 242 |
| **Total** | **413** |

Root meta (outside both trees): `AGENTS.md`, `README.md`, `TODO.md`, this `AUDIT.md`.

---

## Module map (orientation)

| Concern | C++ (`dirtoo/`) | Python (`dirtoo-py/src/dirtoo/`) |
|---------|-----------------|----------------------------------|
| FS locations / FileInfo | `libs/dirtoo-fs/` | `filesystem/` |
| copy/move/rename | `libs/dirops/` + `tools/dt-*` | `posix/`, `programs/` |
| Filter DSL | `libs/dirtoo-filter/` | `filter/`, `expr/`, `find/` |
| Sort/group/visible list | `libs/dirtoo-collection/` | `filecollection/` |
| Directory watch | `libs/dirtoo-watcher/` | `watcher/` |
| Thumbnails | `libs/dirtoo-thumbnail/` | `thumbnail/`, `dbus_thumbnail*` |
| Archives (read) | `libs/dirtoo-archive/` | `archive/` |
| Main GUI | `apps/dirtoo/` | `fileview/`, `gui/` |
| Bookmarks / history | `apps/dirtoo/bookmarks*`, menus | `bookmark/`, `history/` |
| Open-with / MIME | `apps/dirtoo/open_with*` | `mime/`, `xdg_*` |
| CLI utilities | `tools/` | `programs/` |

---

## Inventory — `dirtoo/` (C++)

### `dirtoo/apps`


| Status | Path |
|--------|------|
| ✅ | `dirtoo/apps/dirtoo/CMakeLists.txt` |
| ✅ | `dirtoo/apps/dirtoo/about_dialog.cpp` |
| ✅ | `dirtoo/apps/dirtoo/about_dialog.hpp` |
| ✅ | `dirtoo/apps/dirtoo/app_settings.cpp` |
| ✅ | `dirtoo/apps/dirtoo/app_settings.hpp` |
| ✅ | `dirtoo/apps/dirtoo/badge_icons.hpp` |
| ✅ | `dirtoo/apps/dirtoo/bookmarks.cpp` |
| ✅ | `dirtoo/apps/dirtoo/bookmarks.hpp` |
| ✅ | `dirtoo/apps/dirtoo/clipboard.cpp` |
| ✅ | `dirtoo/apps/dirtoo/clipboard.hpp` |
| ✅ | `dirtoo/apps/dirtoo/conflict_dialog.cpp` |
| ✅ | `dirtoo/apps/dirtoo/conflict_dialog.hpp` |
| ✅ | `dirtoo/apps/dirtoo/directory_load_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/directory_load_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/directory_thumbnail_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/directory_thumbnail_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/directory_tree_model.cpp` |
| ✅ | `dirtoo/apps/dirtoo/directory_tree_model.hpp` |
| ✅ | `dirtoo/apps/dirtoo/drag_action_overlay.cpp` |
| ✅ | `dirtoo/apps/dirtoo/drag_action_overlay.hpp` |
| ✅ | `dirtoo/apps/dirtoo/file_item_delegate.cpp` |
| ✅ | `dirtoo/apps/dirtoo/file_item_delegate.hpp` |
| ✅ | `dirtoo/apps/dirtoo/file_list_model.cpp` |
| ✅ | `dirtoo/apps/dirtoo/file_list_model.hpp` |
| ✅ | `dirtoo/apps/dirtoo/file_views.hpp` |
| ✅ | `dirtoo/apps/dirtoo/filter_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/filter_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/graphics_file_item.cpp` |
| ✅ | `dirtoo/apps/dirtoo/graphics_file_item.hpp` |
| ✅ | `dirtoo/apps/dirtoo/graphics_file_view.cpp` |
| ✅ | `dirtoo/apps/dirtoo/graphics_file_view.hpp` |
| ✅ | `dirtoo/apps/dirtoo/history_menu.cpp` |
| ✅ | `dirtoo/apps/dirtoo/history_menu.hpp` |
| ✅ | `dirtoo/apps/dirtoo/leap_widget.cpp` |
| ✅ | `dirtoo/apps/dirtoo/leap_widget.hpp` |
| ✅ | `dirtoo/apps/dirtoo/location_button_bar.cpp` |
| ✅ | `dirtoo/apps/dirtoo/location_button_bar.hpp` |
| ✅ | `dirtoo/apps/dirtoo/main.cpp` |
| ✅ | `dirtoo/apps/dirtoo/main_window.cpp` |
| ✅ | `dirtoo/apps/dirtoo/main_window.hpp` |
| ✅ | `dirtoo/apps/dirtoo/message_area.cpp` |
| ✅ | `dirtoo/apps/dirtoo/message_area.hpp` |
| ✅ | `dirtoo/apps/dirtoo/name_input_dialog.cpp` |
| ✅ | `dirtoo/apps/dirtoo/name_input_dialog.hpp` |
| ✅ | `dirtoo/apps/dirtoo/open_history.cpp` |
| ✅ | `dirtoo/apps/dirtoo/open_history.hpp` |
| ✅ | `dirtoo/apps/dirtoo/open_with.cpp` |
| ✅ | `dirtoo/apps/dirtoo/open_with.hpp` |
| ✅ | `dirtoo/apps/dirtoo/path_completion_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/path_completion_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/preferences_dialog.cpp` |
| ✅ | `dirtoo/apps/dirtoo/preferences_dialog.hpp` |
| ✅ | `dirtoo/apps/dirtoo/properties_dialog.cpp` |
| ✅ | `dirtoo/apps/dirtoo/properties_dialog.hpp` |
| ✅ | `dirtoo/apps/dirtoo/resources.qrc` |
| ✅ | `dirtoo/apps/dirtoo/search_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/search_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/size_format.cpp` |
| ✅ | `dirtoo/apps/dirtoo/size_format.hpp` |
| ✅ | `dirtoo/apps/dirtoo/sort_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/sort_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/transfer_dialog.cpp` |
| ✅ | `dirtoo/apps/dirtoo/transfer_dialog.hpp` |
| ✅ | `dirtoo/apps/dirtoo/transfer_worker.cpp` |
| ✅ | `dirtoo/apps/dirtoo/transfer_worker.hpp` |
| ✅ | `dirtoo/apps/dirtoo/udisks_client.cpp` |
| ✅ | `dirtoo/apps/dirtoo/udisks_client.hpp` |

### `dirtoo/libs`


| Status | Path |
|--------|------|
| ✅ | `dirtoo/libs/dirops/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirops/cmake/diropsConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirops/include/dirops/error.hpp` |
| ✅ | `dirtoo/libs/dirops/include/dirops/ops.hpp` |
| ✅ | `dirtoo/libs/dirops/include/dirops/util.hpp` |
| ✅ | `dirtoo/libs/dirops/src/error.cpp` |
| ✅ | `dirtoo/libs/dirops/src/ops.cpp` |
| ✅ | `dirtoo/libs/dirops/src/util.cpp` |
| ✅ | `dirtoo/libs/dirtoo-archive/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirtoo-archive/cmake/dirtoo-archiveConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirtoo-archive/include/dirtoo/archive/archive_index.hpp` |
| ✅ | `dirtoo/libs/dirtoo-archive/include/dirtoo/archive/archive_manager.hpp` |
| ✅ | `dirtoo/libs/dirtoo-archive/src/archive_index.cpp` |
| ✅ | `dirtoo/libs/dirtoo-archive/src/archive_manager.cpp` |
| ✅ | `dirtoo/libs/dirtoo-collection/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirtoo-collection/cmake/dirtoo-collectionConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirtoo-collection/include/dirtoo/collection/file_collection.hpp` |
| ✅ | `dirtoo/libs/dirtoo-collection/include/dirtoo/collection/grouper.hpp` |
| ✅ | `dirtoo/libs/dirtoo-collection/include/dirtoo/collection/sorter.hpp` |
| ✅ | `dirtoo/libs/dirtoo-collection/src/file_collection.cpp` |
| ✅ | `dirtoo/libs/dirtoo-collection/src/sorter.cpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirtoo-filter/cmake/dirtoo-filterConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/filter_item.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/match_func.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/media_meta_cache.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/media_probe.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/parser.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/predicates.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/include/dirtoo/filter/search.hpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/src/media_meta_cache.cpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/src/media_probe.cpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/src/parser.cpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/src/predicates.cpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/src/search.cpp` |
| ✅ | `dirtoo/libs/dirtoo-filter/tools/dt_filter.cpp` |
| ✅ | `dirtoo/libs/dirtoo-fs/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirtoo-fs/cmake/dirtoo-fsConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirtoo-fs/include/dirtoo/fs/file_info.hpp` |
| ✅ | `dirtoo/libs/dirtoo-fs/include/dirtoo/fs/location.hpp` |
| ✅ | `dirtoo/libs/dirtoo-fs/src/file_info.cpp` |
| ✅ | `dirtoo/libs/dirtoo-fs/src/location.cpp` |
| ✅ | `dirtoo/libs/dirtoo-thumbnail/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirtoo-thumbnail/cmake/dirtoo-thumbnailConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirtoo-thumbnail/include/dirtoo/thumbnail/thumbnailer.hpp` |
| ✅ | `dirtoo/libs/dirtoo-thumbnail/src/thumbnailer.cpp` |
| ✅ | `dirtoo/libs/dirtoo-watcher/CMakeLists.txt` |
| ✅ | `dirtoo/libs/dirtoo-watcher/cmake/dirtoo-watcherConfig.cmake.in` |
| ✅ | `dirtoo/libs/dirtoo-watcher/include/dirtoo/watcher/directory_watcher.hpp` |
| ✅ | `dirtoo/libs/dirtoo-watcher/src/directory_watcher.cpp` |

### `dirtoo/resources`


| Status | Path |
|--------|------|
| ✅ | `dirtoo/resources/dirtoo.desktop` |
| ✅ | `dirtoo/resources/dirtoo.metainfo.xml` |
| ➖ | `dirtoo/resources/icons/badge-error.png` |
| ➖ | `dirtoo/resources/icons/badge-image.png` |
| ➖ | `dirtoo/resources/icons/badge-loading.png` |
| ➖ | `dirtoo/resources/icons/badge-locked.png` |
| ➖ | `dirtoo/resources/icons/badge-new.png` |
| ➖ | `dirtoo/resources/icons/badge-video.png` |
| ✅ | `dirtoo/resources/icons/crop-thumbnails.svg` |
| ➖ | `dirtoo/resources/icons/dirtoo.png` |
| ✅ | `dirtoo/resources/icons/dirtoo.svg` |
| ➖ | `dirtoo/resources/icons/dnd-ask.png` |
| ➖ | `dirtoo/resources/icons/dnd-copy.png` |
| ➖ | `dirtoo/resources/icons/dnd-link.png` |
| ➖ | `dirtoo/resources/icons/dnd-move.png` |
| ➖ | `dirtoo/resources/icons/dnd-none.png` |
| ✅ | `dirtoo/resources/icons/icon-detail-less.svg` |
| ✅ | `dirtoo/resources/icons/icon-detail-more.svg` |
| ✅ | `dirtoo/resources/icons/view-detail.svg` |
| ✅ | `dirtoo/resources/icons/view-hidden.svg` |
| ✅ | `dirtoo/resources/icons/view-icons.svg` |
| ✅ | `dirtoo/resources/icons/view-sidebar.svg` |
| ✅ | `dirtoo/resources/icons/view-small-icons.svg` |
| ✅ | `dirtoo/resources/icons/zoom-in.svg` |
| ✅ | `dirtoo/resources/icons/zoom-out.svg` |

### `dirtoo/tests`


| Status | Path |
|--------|------|
| ✅ | `dirtoo/tests/CMakeLists.txt` |
| ✅ | `dirtoo/tests/test_archive_index.cpp` |
| ✅ | `dirtoo/tests/test_clipboard_text.cpp` |
| ✅ | `dirtoo/tests/test_collection.cpp` |
| ✅ | `dirtoo/tests/test_dirops.cpp` |
| ✅ | `dirtoo/tests/test_dirops_rename.cpp` |
| ✅ | `dirtoo/tests/test_filter.cpp` |
| ✅ | `dirtoo/tests/test_location.cpp` |

### `dirtoo/tools`


| Status | Path |
|--------|------|
| ✅ | `dirtoo/tools/CMakeLists.txt` |
| ✅ | `dirtoo/tools/cli_common.hpp` |
| ✅ | `dirtoo/tools/dt_archiveinfo.cpp` |
| ✅ | `dirtoo/tools/dt_copy.cpp` |
| ✅ | `dirtoo/tools/dt_mediainfo.cpp` |
| ✅ | `dirtoo/tools/dt_mkdir.cpp` |
| ✅ | `dirtoo/tools/dt_mkfile.cpp` |
| ✅ | `dirtoo/tools/dt_move.cpp` |
| ✅ | `dirtoo/tools/dt_rename.cpp` |
| ✅ | `dirtoo/tools/dt_rm.cpp` |
| ✅ | `dirtoo/tools/dt_rmdir.cpp` |
| ✅ | `dirtoo/tools/dt_swap.cpp` |
| ✅ | `dirtoo/tools/dt_symlink.cpp` |
| ✅ | `dirtoo/tools/json_util.hpp` |

### `dirtoo/`

#### Root

| Status | Path |
|--------|------|
| ✅ | `dirtoo/ARCHITECTURE.md` |
| ✅ | `dirtoo/CMakeLists.txt` |
| ✅ | `dirtoo/README.md` |
| ✅ | `dirtoo/STATUS.md` |
| ✅ | `dirtoo/VERSION` |
| ➖ | `dirtoo/flake.lock` |
| ✅ | `dirtoo/flake.nix` |

---

## Inventory — `dirtoo-py/` (Python reference)

### `dirtoo-py/` — experiments

| Status | Path |
|--------|------|
| ➖ | `dirtoo-py/experiments/README.md` |
| ➖ | `dirtoo-py/experiments/circularreferences/bar.py` |
| ➖ | `dirtoo-py/experiments/circularreferences/circularreferences.py` |
| ➖ | `dirtoo-py/experiments/circularreferences/foo.py` |
| ➖ | `dirtoo-py/experiments/facedetect/face.jpg` |
| ➖ | `dirtoo-py/experiments/facedetect/facedetect.py` |
| ➖ | `dirtoo-py/experiments/filterparser/parser.py` |
| ➖ | `dirtoo-py/experiments/gaussian/gaussian.py` |
| ➖ | `dirtoo-py/experiments/inotify/inotify.py` |
| ➖ | `dirtoo-py/experiments/popup/popup.py` |
| ➖ | `dirtoo-py/experiments/pyqtcrash/pyqtcrash.py` |
| ➖ | `dirtoo-py/experiments/pyqttest/pyqttest.py` |
| ➖ | `dirtoo-py/experiments/qgraphicperf/qgraphicperf.py` |
| ➖ | `dirtoo-py/experiments/qmime/qmime.py` |
| ➖ | `dirtoo-py/experiments/qmltest/.gitignore` |
| ➖ | `dirtoo-py/experiments/qmltest/main.qml` |
| ➖ | `dirtoo-py/experiments/qmltest/qmltest.py` |
| ➖ | `dirtoo-py/experiments/qnotify/qnotify.py` |
| ➖ | `dirtoo-py/experiments/qtinotify/qtinotify.py` |
| ➖ | `dirtoo-py/experiments/threadtest/threadtest.py` |
| ➖ | `dirtoo-py/experiments/udisks/udisks.py` |
| ➖ | `dirtoo-py/experiments/udisks/udisksqt.py` |

### `dirtoo-py/` — src

| Status | Path |
|--------|------|
| ✅ | `dirtoo-py/src/dirtoo/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/archive_extractor.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/archive_manager.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/archiveinfo.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/extractor.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/extractor_factory.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/libarchive_extractor.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/rar_extractor.py` |
| ✅ | `dirtoo-py/src/dirtoo/archive/sevenzip_extractor.py` |
| ✅ | `dirtoo-py/src/dirtoo/bookmark/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/bookmark/bookmarks.py` |
| ✅ | `dirtoo-py/src/dirtoo/bookmark/bookmarks_provider.py` |
| ✅ | `dirtoo-py/src/dirtoo/dbus_thumbnail_cache.py` |
| ✅ | `dirtoo-py/src/dirtoo/dbus_thumbnailer.py` |
| ✅ | `dirtoo-py/src/dirtoo/duration.py` |
| ✅ | `dirtoo-py/src/dirtoo/expr/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/expr/expr.py` |
| ✅ | `dirtoo-py/src/dirtoo/ffprobe.py` |
| ✅ | `dirtoo-py/src/dirtoo/file_transfer.py` |
| ✅ | `dirtoo-py/src/dirtoo/file_type.py` |
| ✅ | `dirtoo-py/src/dirtoo/filecollection/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/filecollection/file_collection.py` |
| ✅ | `dirtoo-py/src/dirtoo/filecollection/filter.py` |
| ✅ | `dirtoo-py/src/dirtoo/filecollection/grouper.py` |
| ✅ | `dirtoo-py/src/dirtoo/filecollection/sorter.py` |
| ✅ | `dirtoo-py/src/dirtoo/filesystem/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/filesystem/file_info.py` |
| ✅ | `dirtoo-py/src/dirtoo/filesystem/lazy_file_info.py` |
| ✅ | `dirtoo-py/src/dirtoo/filesystem/location.py` |
| ✅ | `dirtoo-py/src/dirtoo/filesystem/stdio_filesystem.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/actions.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/application.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/application_actions.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/controller.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/executor.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/file_graphics_scene.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/file_item.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/file_item_renderer.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/file_view.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/file_view_style.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/file_view_window.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/filelist_stream.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/filesystem_operations.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/gnome.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/gui.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/layout.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/layout_builder.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/mode.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/path_completion.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/rename_operation.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/return_value.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/scaler.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/search_stream.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/settings.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/virtual_filesystem.py` |
| ✅ | `dirtoo-py/src/dirtoo/fileview/worker_thread.py` |
| ✅ | `dirtoo-py/src/dirtoo/filter/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/filter/filter_command_parser.py` |
| ✅ | `dirtoo-py/src/dirtoo/filter/filter_expr_parser.py` |
| ✅ | `dirtoo-py/src/dirtoo/filter/filter_parser.py` |
| ✅ | `dirtoo-py/src/dirtoo/filter/match_func.py` |
| ✅ | `dirtoo-py/src/dirtoo/filter/match_func_factory.py` |
| ✅ | `dirtoo-py/src/dirtoo/find/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/find/action.py` |
| ✅ | `dirtoo-py/src/dirtoo/find/context.py` |
| ✅ | `dirtoo-py/src/dirtoo/find/filter.py` |
| ✅ | `dirtoo-py/src/dirtoo/find/util.py` |
| ✅ | `dirtoo-py/src/dirtoo/find/walk.py` |
| ✅ | `dirtoo-py/src/dirtoo/format.py` |
| ✅ | `dirtoo-py/src/dirtoo/fuzzy.py` |
| ✅ | `dirtoo-py/src/dirtoo/glob.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/about_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/conflict_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/create_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/directory_context_menu.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/drag_widget.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/filter_line_edit.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/history_menu.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/item_context_menu.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/label.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/leap_widget.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/location_buttonbar.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/location_lineedit.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/menu.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/message_area.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/preferences_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/properties_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/push_button.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/rename_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/search_line_edit.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/tool_button.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/transfer_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/transfer_error_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/gui/transfer_request_dialog.py` |
| ✅ | `dirtoo-py/src/dirtoo/history/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/history/history.py` |
| ✅ | `dirtoo-py/src/dirtoo/history/history_provider.py` |
| ✅ | `dirtoo-py/src/dirtoo/icons/README.md` |
| ✅ | `dirtoo-py/src/dirtoo/icons/compress.gif` |
| ➖ | `dirtoo-py/src/dirtoo/icons/dirtoo.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/dirtoo.svg` |
| ➖ | `dirtoo-py/src/dirtoo/icons/dnd-ask.png` |
| ➖ | `dirtoo-py/src/dirtoo/icons/dnd-copy.png` |
| ➖ | `dirtoo-py/src/dirtoo/icons/dnd-link.png` |
| ➖ | `dirtoo-py/src/dirtoo/icons/dnd-move.png` |
| ➖ | `dirtoo-py/src/dirtoo/icons/dnd-none.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/gears.gif` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_175057_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_175057_cc.xcf` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_236873_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_236873_cc.xcf` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_258297_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_258297_cc.xcf` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_36746_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_36746_cc.xcf` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_386758_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_386758_cc.xcf` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_409399_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_409399_cc.xcf` |
| ➖ | `dirtoo-py/src/dirtoo/icons/noun_757280_cc.png` |
| ✅ | `dirtoo-py/src/dirtoo/icons/noun_757280_cc.xcf` |
| ✅ | `dirtoo-py/src/dirtoo/icons/scan.gif` |
| ✅ | `dirtoo-py/src/dirtoo/icons/search.gif` |
| ✅ | `dirtoo-py/src/dirtoo/image/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/image/icon.py` |
| ✅ | `dirtoo-py/src/dirtoo/image/image_filter.py` |
| ✅ | `dirtoo-py/src/dirtoo/list_dict.py` |
| ✅ | `dirtoo-py/src/dirtoo/mediainfo.py` |
| ✅ | `dirtoo-py/src/dirtoo/metadata/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/metadata/metadata.py` |
| ✅ | `dirtoo-py/src/dirtoo/metadata/metadata_cache.py` |
| ✅ | `dirtoo-py/src/dirtoo/metadata/metadata_collector.py` |
| ✅ | `dirtoo-py/src/dirtoo/mime/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/mime/mime_database.py` |
| ✅ | `dirtoo-py/src/dirtoo/posix/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/posix/filesystem.py` |
| ✅ | `dirtoo-py/src/dirtoo/profiler/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/profiler/profiler.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/archive_extractor.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/archiveinfo.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/chomp.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/desktop.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/dirtool.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/expr.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/fileview.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/find.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/fsck.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/fuzzy.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/glob.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/guessarchivename.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/guitest.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/icon.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/mediainfo.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/metadata.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/mime.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/mkevil.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/mktest.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/move.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/rmdir.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/shuffle.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/sleep.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/swap.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/thumbnailer.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/unidecode.py` |
| ✅ | `dirtoo-py/src/dirtoo/programs/watch.py` |
| ✅ | `dirtoo-py/src/dirtoo/sort.py` |
| ✅ | `dirtoo-py/src/dirtoo/stream/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/stream/stream_manager.py` |
| ✅ | `dirtoo-py/src/dirtoo/tee_io.py` |
| ✅ | `dirtoo-py/src/dirtoo/thumbnail/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/thumbnail/directory_thumbnailer.py` |
| ✅ | `dirtoo-py/src/dirtoo/thumbnail/thumbnail.py` |
| ✅ | `dirtoo-py/src/dirtoo/thumbnail/thumbnailer.py` |
| ✅ | `dirtoo-py/src/dirtoo/unique.py` |
| ✅ | `dirtoo-py/src/dirtoo/util.py` |
| ✅ | `dirtoo-py/src/dirtoo/watcher/__init__.py` |
| ✅ | `dirtoo-py/src/dirtoo/watcher/archive_directory_watcher.py` |
| ✅ | `dirtoo-py/src/dirtoo/watcher/directory_watcher.py` |
| ✅ | `dirtoo-py/src/dirtoo/watcher/directory_watcher_worker.py` |
| ✅ | `dirtoo-py/src/dirtoo/watcher/inotify_qt.py` |
| ✅ | `dirtoo-py/src/dirtoo/xdg_desktop.py` |
| ✅ | `dirtoo-py/src/dirtoo/xdg_mime_associations.py` |

### `dirtoo-py/` — tests

| Status | Path |
|--------|------|
| ✅ | `dirtoo-py/tests/__init__.py` |
| ✅ | `dirtoo-py/tests/test.7z` |
| ✅ | `dirtoo-py/tests/test.mkv` |
| ✅ | `dirtoo-py/tests/test.rar` |
| ✅ | `dirtoo-py/tests/test_cmd_metadata.py` |
| ✅ | `dirtoo-py/tests/test_duration.py` |
| ✅ | `dirtoo-py/tests/test_expr.py` |
| ✅ | `dirtoo-py/tests/test_ffprobe.py` |
| ✅ | `dirtoo-py/tests/test_file_info.py` |
| ✅ | `dirtoo-py/tests/test_fileview_thumbnailer.py` |
| ✅ | `dirtoo-py/tests/test_filter_parser.py` |
| ✅ | `dirtoo-py/tests/test_format.py` |
| ✅ | `dirtoo-py/tests/test_incomplete.7z` |
| ✅ | `dirtoo-py/tests/test_incomplete.rar` |
| ✅ | `dirtoo-py/tests/test_list_dict.py` |
| ✅ | `dirtoo-py/tests/test_location.py` |
| ✅ | `dirtoo-py/tests/test_mediainfo.py` |
| ✅ | `dirtoo-py/tests/test_metadata_collector.py` |
| ✅ | `dirtoo-py/tests/test_rar_extractor_worker.py` |
| ✅ | `dirtoo-py/tests/test_sevenzip_extractor_worker.py` |
| ✅ | `dirtoo-py/tests/test_tee_io.py` |
| ✅ | `dirtoo-py/tests/test_util.py` |

### `dirtoo-py/` — Root

| Status | Path |
|--------|------|
| ✅ | `dirtoo-py/.gitattributes` |
| ✅ | `dirtoo-py/.gitignore` |
| ✅ | `dirtoo-py/LICENSE.txt` |
| ✅ | `dirtoo-py/README.md` |
| ✅ | `dirtoo-py/VERSION` |
| ✅ | `dirtoo-py/dirhier.py` |
| ✅ | `dirtoo-py/dirtoo.desktop` |
| ✅ | `dirtoo-py/dirtoo.nix` |
| ➖ | `dirtoo-py/flake.lock` |
| ✅ | `dirtoo-py/flake.nix` |
| ✅ | `dirtoo-py/pyproject.toml` |
| ✅ | `dirtoo-py/setup.py` |

---

## Per-file notes

_Filled as each file is reviewed. Newest notes at the bottom of each subsection._

### C++ notes

#### `dirtoo/libs/dirtoo-fs/` (2026-08-02)

Foundation library: locations + directory listing metadata. No Qt. Consumed by almost every other lib and the GUI.

##### `location.hpp` / `location.cpp` ✅

**Role**
- `Location` models a browsable place: plain `file` path or `archive` (archive file + optional entry path).
- URL forms: preferred Python-style `file:///path.zip//archive[:entry]`; legacy `archive:///path!/entry` still parsed.
- Factories: `from_path` (weakly_canonical), `from_path_unchecked` (listing hot path), `from_archive`, `from_url`, `from_human`.
- Navigation: `parent()`, `join()`, `basename()`, `dirname()`, `as_url()`, `as_path()`.
- `looks_like_archive()` extension heuristic (zip/tar/gz/7z/rar/… + compound `.tar.*`).
- `std::hash<Location>` via `as_url()`.

**Issues / risks**
- **Percent-encoding is incomplete**: only encodes/decodes space (`%20`). Paths with `#`, `?`, non-ASCII, or other reserved characters can break URL round-trips and thumbnail MD5 keys that hash `as_url()`.
- **`from_url` throws** `std::invalid_argument` on unknown schemes — callers must try/catch (GUI location bar should not assume infallible parse).
- **`looks_like_archive`**: lone `.gz`/`.bz2`/`.xz` match any file with that extension (including non-archives); acceptable heuristic but can false-positive.
- **`empty()`** only checks `path_.empty()`; a default-constructed Location is empty, but a root `/` is not — fine; no `is_valid()` distinct from empty.
- Nested multi-payload locations (Python can stack payloads) are **not modeled** — only one archive layer. Matches current C++ archive browser scope.

**Parity vs Python `filesystem/location.py`**
- Python supports multi-payload stack and extra protocols (e.g. `search://` with query payload). C++ intentionally narrower: file + single archive level.
- Python `Location.from_search_query` / search locations: **missing on C++** (search is implemented as a GUI mode over real paths, not a Location protocol).

##### `file_info.hpp` / `file_info.cpp` ✅

**Role**
- Snapshot of one entry: location, display name, size, mtime, dir/file/symlink flags, permissions, synthetic flag.
- Builders: `from_path`, `from_location`, `from_directory_entry` (cheap listing), `synthetic` (archive members / virtual rows).
- `list_directory(Location)` — non-recursive, local FS only; skips permission-denied; **returns empty for archives** (archive listing lives in `dirtoo-archive`).

**Issues / risks**
- **Symlink directories**: uses `symlink_status` / `is_directory(status)` without following — good for not traversing link loops; UI may need explicit “link to dir” affordance (flag is exposed).
- **`from_location` for archives** always synthesizes `is_directory=false`, size 0 — wrong if caller expected a directory entry inside an archive; real archive rows should use `synthetic(...)` with correct flags from the archive index.
- **`list_directory` swallows errors** (clears `ec` and continues) — no error channel to GUI; empty result can mean empty dir *or* unreadable path.
- No owner/group, no device/inode, no access-time — enough for browser; Properties dialog may need more later.
- Default `mtime_{}` zero epoch if stat fails — UI should treat carefully.

**Parity vs Python**
- Python `FileInfo` + `LazyFileInfo` defer stat and carry richer metadata hooks. C++ eagerly stats on construct (except synthetic). Listing path is optimized via `from_directory_entry`.
- Python `StdioFilesystem` coordinates archive extract cache + watchers — **not in dirtoo-fs**; split across `dirtoo-archive`, `dirtoo-watcher`, and app code. Cleaner modularity; ensure no feature left only in Python’s stdio layer without a C++ home.

##### Build files ✅

- Standalone CMake package, C++23, install + config package. No issues for audit scope.

---


#### `dirtoo/libs/dirops/` (2026-08-02)

Qt-free filesystem mutation library. GUI and CLI tools must call this instead of ad-hoc `std::filesystem` writes.

##### API surface (`ops.hpp`, `error.hpp`, `util.hpp`) ✅

| Op | Purpose |
|----|---------|
| `copy_path` | File or recursive directory; symlinks copied as links |
| `move_path` | Rename same-FS or copy+delete cross-device |
| `rename_path` | Name change with conflict policy |
| `remove_path` | `remove_all` (recursive) |
| `create_directory` | Single directory (not parents) |
| `create_file` | Empty regular file |
| `create_symlink` | Symlink at link path |
| `swap_names` | Atomic-ish swap on same filesystem |
| `unique_path` | `file (2).txt` style conflict rename |
| `same_filesystem` | Device comparison for move strategy |

**Options:** `dry_run`, `verbose`, `ConflictPolicy` {Fail, Overwrite, Rename, Skip}, `on_progress`, `is_cancelled`.

**Errors:** `std::expected<Result, Error>` — no exceptions across API for ordinary failures (`Error` holds `error_code`, path, message).

##### Implementation notes / issues (`ops.cpp` ~587 lines)

- **Progress / cancel** wired for multi-file copy paths; verify every loop polls `is_cancelled` (audit: present on recursive copy; confirm single-file paths).
- **Cross-device move**: copy then `remove_all` source — if copy succeeds and delete fails, data is duplicated (documented risk; no transaction).
- **`create_directory`** does not create parents (unlike `dt-mkdir -p` which uses `create_directories` in the tool). Consistent with POSIX `mkdir` vs `mkdir -p`.
- **Directory merge on move into existing dir** — behavior depends on resolve_destination; worth a dedicated test against Python `dt-move` merge semantics.
- Good test coverage in `tests/test_dirops.cpp` (conflicts, dry-run, swap, symlink).

##### Build ✅

Standalone package; CLI tools link this from `tools/` and flake `dirops` derivation.

---

#### `dirtoo/libs/dirtoo-filter/` (2026-08-02)

Filter DSL, recursive search walk, and media metadata (ffprobe + SQLite cache). No Qt. ~3.6k LOC.

##### Core types ✅

| Piece | Role |
|-------|------|
| `FilterItem` | Minimal match input: name, size, is_directory, path, optional mtime |
| `MatchFunc` hierarchy | AlwaysTrue/False, And/Or/Not, shared_ptr composition |
| `parse_filter` | Recursive-descent DSL → MatchFunc; `std::expected` + position |
| `search_directory` | Cooperative walk with depth/hidden/symlink options |
| `MediaMetaCache` | Async SQLite + ffprobe; generation bump for cancel |
| `probe_media` | Sync ffprobe (CLI / workers) |
| `dt_filter` | CLI tool |

##### DSL commands (parity with Python factory)

Implemented: `glob`/`Glob`, `regex`/`Regex`, `type`/`t`, `size`, `width`, `height`, `duration`, `framerate`, `fuzzy`, `length`/`len`, `date`, `time`, `weekday`, `contains`/`Contains`, `containsre`/`Containsre`, `containsfuzzy`, `random`, `charset`, `pages`, `filecount`.

Boolean: `and`/`or`/`not`, juxtaposition = AND, `-`/`^` unary not, parentheses.

Bare words → case-insensitive substring via implicit `*word*` glob.

##### Issues / risks

- **`FilterItem` has no `is_symlink`** — `type:link` must `lstat`/`is_symlink` on path inside the predicate (extra I/O; OK if done carefully). Confirm implementation does not rely on a missing flag alone.
- **Content predicates** (`contains*`) read up to 1 MiB synchronously when matching — GUI uses FilterWorker for content filters; **search** and **dt-filter** can block if used carelessly on huge trees.
- **Media predicates** (width/height/duration/…) depend on cache/probe; on cache miss may return false until async fill — GUI must refresh when meta arrives (known architecture).
- **Percent / URL encoding** not relevant here; paths are filesystem paths.
- **Parser** documents “fixes missing parentheses from the Python DSL” — intentional stricter grammar; some Python expressions with ambiguous juxtaposition may need parens on C++.
- **Python `pick:`** was commented out in factory — still absent on C++; fine.
- **`expr/` package** in Python is a separate expression evaluator; C++ folded logic into `parser` + `predicates`.
- **`find/` package** (walk + actions) ≈ C++ `search_directory` without Python’s action pipeline (print/delete/etc. CLI actions) — OOS unless needed for tools.

##### Tests

`tests/test_filter.cpp` covers parser/predicates; keep media/cache edge cases under regression when touching SQLite schema.

---

#### `dirtoo/apps/dirtoo/` — GUI (2026-08-02)

Largest surface (~13k LOC). Review is by **feature group**; `main_window.cpp` (~4200 lines) is the integration hub.

##### Shell / entry
| Files | Role |
|-------|------|
| `main.cpp` | QApplication, logging handler, CLI verbose/debug, **GnomeButtonOrderStyle** |
| `CMakeLists.txt` / `resources.qrc` | App target, SVG/PNG install, Qt Svg |
| `app_settings.*` | QSettings persistence: view mode, zoom, icon detail, crop, hidden, filter, sidebar, group, size units, geometry, location history |
| `badge_icons.hpp` | Bundled badge/icon load + missing-asset warnings |
| `size_format.*` | SI/IEC size strings |

##### Navigation & location
| Files | Role |
|-------|------|
| `location_button_bar.*` | Breadcrumb bar, drag targets |
| `path_completion_worker.*` | Async path completion for location edit |
| `directory_load_worker.*` | Off-thread directory listing |
| `leap_widget.*` | Quick jump UI |
| `history_menu.*` | Menu that tracks middle-click |
| `bookmarks.*` | Bookmark store |
| `open_history.*` | File-open history (app + paths), dialog, recent menu |

**Present:** parent/home/back/forward, history, bookmarks, open location from path/URL, archive locations via archive manager, path completion.

##### Views & models
| Files | Role |
|-------|------|
| `file_list_model.*` | Qt model over FileCollection visible items |
| `file_item_delegate.*` | Detail/list painting, thumbs, badges |
| `file_views.hpp` | Shared view helpers |
| `graphics_file_view.*` / `graphics_file_item.*` | Icon/graphics view |
| `filter_worker.*` / `sort_worker.*` | Content filter + sort off UI thread |
| `directory_tree_model.*` | Sidebar tree |
| `directory_thumbnail_worker.*` | Folder collage thumbs |

**Present:** Detail / Icons / Small icons, zoom, icon detail levels, crop thumbs, group modes, natural sort + media sorts.

##### Clipboard / transfer / DnD
| Files | Role |
|-------|------|
| `clipboard.*` | Copy/cut URIs + text |
| `transfer_worker.*` / `transfer_dialog.*` | Progress UI + dirops-backed work |
| `conflict_dialog.*` | Replace / Rename / Skip / Cancel |
| `drag_action_overlay.*` | DnD action cursor/overlay |

**Present:** copy, cut, paste, paste-as-link, DnD with drop target directory, conflict policy.

##### Open with / properties / dialogs
| Files | Role |
|-------|------|
| `open_with.*` | Desktop apps, default open, command dialog, terminal, history record |
| `properties_dialog.*` | Metadata display (permissions **display**, not edit) |
| `preferences_dialog.*` | App prefs |
| `about_dialog.*` / `name_input_dialog.*` | About, rename/mkdir name entry |
| `message_area.*` | Status/message strip |

##### Devices / search
| Files | Role |
|-------|------|
| `udisks_client.*` | Volume list (UDisks2) |
| `search_worker.*` | Recursive search via `dirtoo-filter::search_directory` |

##### Feature presence vs Python gaps (GUI-focused)

| Feature | C++ | Notes |
|---------|-----|-------|
| Three view modes + zoom | Yes | |
| Filter + pinned filter + help | Yes | |
| Recursive search | Yes | Results in collection, not `search://` Location |
| Bookmarks / location history | Yes | |
| Recently Opened + open history | Yes | C++ addition / strengthened |
| Clipboard + Link paste + DnD | Yes | |
| Transfers + conflicts | Yes | |
| Sidebar tree + Devices | Yes | Mount/eject: check `udisks_client` completeness |
| Thumbnails + dir thumbs | Yes | Archive file thumbs weak |
| Archive browse | Yes | Via ArchiveManager |
| Undo | **No** | Python also largely stub-level historically |
| Editable permissions | **No** | Display only |
| Virtual FS abstraction | **No** | Python `virtual_filesystem.py` — C++ uses Location + archive extract |
| Kinetic/layout builder complexity | **No** | Python layout modules richer; C++ graphics view simpler |
| Executor / action controller split | Partial | Logic concentrated in `MainWindow` |

**Architecture smell:** `main_window.cpp` is very large — controllers, menus, transfer, search, devices, and view wiring live together. Not a functional bug, but future work should peel controllers (transfer, search, navigation) into helpers to match Python’s `controller` / `actions` split.

**GUI I/O rule:** Directory load, sort, content filter, path completion, search, transfer, thumbnails are worker-based. Residual risk: some property/stat paths and filter non-content matching still on UI thread (acceptable for name filters).

---

#### `dirtoo/tools/` + `dirtoo/tests/` (2026-08-02)

##### CLI tools
| Tool | Backend |
|------|---------|
| `dt_copy/move/rename/rm/mkdir/mkfile/symlink/swap` | `dirops` |
| `dt_rmdir` | app-linked / collection side |
| `dt_mediainfo` | filter media probe |
| `dt_archiveinfo` | archive index |
| `dt_filter` | lives under `dirtoo-filter/tools` |

Shared: `cli_common.hpp`, `json_util.hpp`.

**vs Python `programs/`:** Large Python toolbox (fsck, shuffle, desktop, fuzzy, find, …) is **mostly OOS** for the C++ port. C++ keeps the mutation helpers + media/archive/filter CLIs that support the GUI and testing.

##### Tests (Catch2)
`test_location`, `test_filter`, `test_collection`, `test_dirops` (+ rename), `test_archive_index`, `test_clipboard_text`.

**Gaps:** No automated GUI tests; limited archive/media integration tests; no watcher/thumbnail unit tests (Qt/D-Bus heavy).

---

---

## Deep per-file review (pass 2 — 2026-08-02)

This section revisits the inventory with **implementation-level** notes: call
graphs, GUI-thread risk, error paths, and concrete parity gaps. Inventory
checkmarks alone are not a review.

### Reading convention

| Tag | Meaning |
|-----|---------|
| **Role** | What the file owns |
| **API / flow** | Key types, signals, call sequence |
| **Issues** | Bugs, smells, incomplete bits, risks |
| **Parity** | Python counterpart + delta |
| **Tests** | Coverage status |

---

### `dirtoo/apps/dirtoo/` — application

#### `main.cpp` ✅

**Role.** Process entry: `QApplication`, logging handler, CLI (`QCommandLineParser`), GNOME button-box layout proxy, optional path args → `MainWindow::open_location`.

**API / flow.** Installs `dirtoo_message_handler` with level gate (`--verbose` / `--debug`). Style: `QProxyStyle` forces `QDialogButtonBox::GnomeLayout`. Opens `MediaMetaCache` DB before windows. Sets app name/version from `DIRTOO_VERSION`.

**Issues.**
- Multiple windows only via GUI (`open_new_window`); CLI opens one window.
- Logging context needs `QT_MESSAGELOGCONTEXT` at compile time for file:line (documented in handler).
- No single-instance / D-Bus activation (Python also mostly multi-window via new controller).

**Parity.** Python `fileview/application.py` is heavier (thumbnailer, metadata collector, executor as app services). C++ constructs those on `MainWindow` / libraries instead.

**Tests.** None (manual).

---

#### `main_window.hpp` / `main_window.cpp` (~4.6k LOC) ✅

**Role.** Central orchestrator: views, navigation, filter/search, clipboard transfers, archives, sidebar, menus, settings persistence.

**API / flow (subsystems).**
1. **Navigation** — `open_location` → stop search → archive branch or `DirectoryLoadWorker` → `FileCollection` → `SortWorker` / `FilterWorker` → `FileListModel::refresh` → thumbnails.
2. **Views** — `QStackedWidget`: Detail `QTreeView`, Icons `GraphicsFileView`, List `QListView`; shared `FileListModel` + `FileItemDelegate`.
3. **Transfers** — `TransferController` + conflict dialogs; mutations via **dirops** only.
4. **Archives** — `ArchiveManager::open` + TOC via `list_archive_entries` / `fileinfos_for_prefix`; read-only UI guards.
5. **Sidebar** — `DirectoryTreeModel` + Places + `UDisksClient` Devices.
6. **Status** — left `status_label_` (filename / messages), right `status_info_label_` (visible/total/selected sizes).

**Issues.**
- **God-object**: still owns menus, workers, archive state, device actions, open-with wiring. Controllers (`NavigationHistory`, `SearchController`, `TransferController`) peel some load but menus/actions remain.
- **Soft watcher reload** still full `readdir` + merge; no inotify per-entry invalidate.
- **Search** synthetic rows lack full metadata until reopen; interval `refresh_list` can still jank on huge result sets.
- **Archive thumbs** extract-on-demand via `QtConcurrent` + alias map — correct but complex failure paths.
- Residual GUI I/O: non-content filter `rebuild_visible`, some `QFileIconProvider` / `stat` for properties.

**Parity.** Python `Controller` + `FileViewWindow` + `FileViewApplication`. C++ folds controller into MainWindow; search is not a `search://` Location.

**Tests.** Indirect via unit tests of libs; no MainWindow tests.

---

#### `file_list_model.hpp` / `.cpp` (~774 LOC) ✅

**Role.** `QAbstractTableModel` over `FileCollection::visible_items()`. Columns: Name, Size, Width/Height/Dimensions/AspectRatio/Framerate, Duration, Modified, Type. Extra roles: path, group, thumbnail status, access denied, is-new, time-gap, child count.

**API / flow.** Decoration for icons (XDG path key or Location URL for archive members). `request_child_count` async for folders. Media columns pull `MediaMetaCache::try_get` / `request` (no GUI ffprobe).

**Issues.**
- Size for directories shows `st_size` + optional “N items” — not recursive du (by design; matches recent product choice).
- Archive members: size depends on verbose archive listing (now `-tvf`/`unzip -l`).
- `mimeData` emits archive Location URLs — external apps may not understand; internal DnD OK.

**Parity.** Python model is more diffuse (FileCollection + scene items). C++ consolidates Qt roles here.

---

#### `file_item_delegate.hpp` / `.cpp` (~564 LOC) ✅

**Role.** Paints Detail + List icon cells: thumbnails, crop mode, directory montage overlay (whitened full-size folder, hide on hover), media badges, duration/dimensions text, loading/error stickers, group headers, time gaps.

**Issues.**
- Error sticker policy now gated in MainWindow fail handler; delegate still paints Failed if set.
- Hover depends on view mouse-tracking / `State_MouseOver` (Detail/List).
- Icon path and graphics path can drift (must keep montage/badge logic twin).

**Parity.** Python `file_item_renderer.py` + style. C++ lacks white_outline glow on folder icon.

---

#### `graphics_file_view.hpp` / `.cpp` (~956 LOC) + `graphics_file_item.*` ✅

**Role.** Icons mode: `QGraphicsView` scene with **viewport-windowed** `GraphicsFileItem` tiles (reuse off-screen). Layout slots, group headers, keyboard file cursor, DnD with modifier actions, drag_entered_ guard, select-all across model rows.

**Issues.**
- Windowed materialization is correct for large dirs; selection persistence across scroll needs careful `select_all` (implemented).
- DnD drop into folder uses item under cursor; nested-drop guard lives in MainWindow.
- Item paint must stay aligned with delegate (directory overlay, badges).

**Parity.** Python `FileView` + `FileItem` + scene. Python layout builders more elaborate (kinetic); C++ grid is simpler and faster.

---

#### `file_views.hpp` ✅

**Role.** Thin typedefs / helpers for view wiring if any. Low complexity.

---

#### Workers

| File | Role | Issues |
|------|------|--------|
| `directory_load_worker.*` | Off-thread `list_directory` + generation token | Archive path must pass resolved extract dir from MainWindow |
| `sort_worker.*` | Off-thread sort copy of items | Content-filter path must not call `replace_items_sorted` rebuild that runs content matchers on GUI |
| `filter_worker.*` | Content filters (`contains*`) off GUI | Name-only filters still sync on GUI via collection |
| `search_worker.*` | Recursive `filter::search_directory` | Emits every match; GUI must batch |
| `path_completion_worker.*` | Dir-only completions + request id | Cancel/stale id handling present |
| `directory_thumbnail_worker.*` | Montage generation | Only on explicit user action (not auto) |

**Parity.** Python QThread workers for metadata/thumbs/search; C++ matches the spirit with generation tokens.

---

#### Dialogs & chrome

| File | Role | Issues / parity |
|------|------|-----------------|
| `about_dialog.*` | Version, license, project URL | Uses real URL; version from QApplication |
| `preferences_dialog.*` | Subset of AppSettings | Not full Python prefs surface |
| `properties_dialog.*` | Multi-file props, media meta | Read-only permissions; no chmod |
| `conflict_dialog.*` | Replace policy + thumbs | GNOME button order; thumb from cache/icon |
| `transfer_dialog.*` | Progress UI | Pause/cancel wired to worker |
| `name_input_dialog.*` | Rename / new file/folder | Validation thin (empty name) |
| `message_area.*` | Inline error/info strip | Not status bar; separate from permanent size label |
| `location_button_bar.*` | Breadcrumbs + DnD onto segments | Archive segment: package icon (no `[archive]` text) |
| `leap_widget.*` | Type-ahead leap overlay | Mirrors Python leap |
| `drag_action_overlay.*` | Cursor badges copy/move/link | Theme/bundled PNG |
| `history_menu.*` | Back/forward overflow menus | Icons via `icon_for_location` |
| `badge_icons.hpp` | Resolve icon dir + load | Multiple search paths; warns once |

---

#### Clipboard / history / open

| File | Role | Issues |
|------|------|--------|
| `clipboard.*` | dirtoo + GNOME + uri-list MIME | Link mode supported; tested parsers |
| `bookmarks.*` | URL-per-line file under AppConfig | Sorted unique; toggle |
| `navigation_history.*` | Back/forward stack | Distinct from persistent location history in settings |
| `open_history.*` | SQLite recently opened | Stronger than Python’s simple list |
| `open_with.*` | XDG desktop apps, Exec expand | Spec incomplete (no StartupWMClass etc.); multi-file `%F`/`%f` handled |
| `operations_history.*` | Append-only SQLite log of mutations | **Not undo** — browser dialog only |
| `app_settings.*` | QSettings load/save | Zoom per view; detail columns; size SI/IEC |
| `size_format.*` | SI/IEC byte formatting | Shared by model, status, conflict dialog |

---

#### Sidebar / volumes

| File | Role | Issues |
|------|------|--------|
| `directory_tree_model.*` | Lazy tree, Places roots, `fetchMore` + generation | Deep expand races mitigated by generation; hidden dirs optional |
| `udisks_client.*` | List FS, Mount/Unmount/Eject async | QDBus `ay` mount points; no format/partition; requires system bus |

---

#### Controllers

| File | Role |
|------|------|
| `search_controller.*` | Thread lifecycle for SearchWorker |
| `transfer_controller.*` | Thread + dialog session for TransferWorker |
| `transfer_worker.*` | Sequential dirops ops, progress, conflict callbacks |

**Issues — `transfer_worker` conflict path (code-backed).**
- Uses `conflict_mutex_` + `conflict_cv_`: worker emits `conflict_required`, then waits until `resolve_conflict()` sets `conflict_answer_` / `conflict_accepted_`.
- **Deadlock risk** if MainWindow is destroyed or the dialog never calls `resolve_conflict` (cancel path must always notify the CV — `cancel()` clears `conflict_pending_` and notifies).
- Pause uses a separate `pause_cv_`; cancel unblocks both pause and conflict waits.
- Each item is a single dirops call; progress depends on dirops `on_progress` for multi-file trees.

**Issues — `search_controller`.**
- Owns worker thread; MainWindow must not touch worker objects after `shutdown`.
- Match flood: worker emits per hit; MainWindow batches model refresh (every 128).

---

#### `CMakeLists.txt` / `resources.qrc` ✅

App target links fs, collection, filter, watcher, thumbnail, archive, dirops, Qt6, SQLite. Icons installed to `share/dirtoo/icons`; `DIRTOO_ICON_DIR` for build tree. `resources.qrc` may be residual if icons are filesystem-based — check for unused embeds.

---

### `dirtoo/libs/` — libraries

#### `dirtoo-fs` (see pass-1 notes; addenda)

**Issues (addenda).**
- `list_directory` does not follow symlink-to-dir as separate type beyond `is_symlink` + `is_directory` via status.
- Synthetic archive entries share container `path()` for some code paths — model keys must use Location URL.

#### `dirops` (see pass-1; addenda)

**Issues (addenda).**
- `swap_names` requires same filesystem; failure mode clear.
- Overwrite policy on directories is destructive (`remove_all`) — conflict dialog must not offer Overwrite lightly for dirs.

#### `dirtoo-collection`

**Role.** `FileCollection`: items + visible, filter expression, show_hidden, sorter, grouper (None/Day/Directory/Duration).

**Issues.**
- `rebuild_visible` (collection.cpp): walks all `items_`, applies `show_hidden_` + `match_->matches(to_filter_item(fi))`, optional `stable_sort` by `group_key`. **Runs on whatever thread calls it** — MainWindow must only call it on the GUI thread for cheap matchers; content matchers must use FilterWorker + `replace_visible`.
- `merge_items`: soft watcher path keeps order of survivors, updates metadata, appends new — avoids full reshuffle flicker when `rebuild` deferred.
- Duration grouping reads `MediaMetaCache::try_get` only (no probe on GUI) — unknown bucket until meta arrives; `refresh_groups` after meta.
- Natural sort in sorter (`numeric_sort_key`) — tested in `test_collection`.

**Parity.** Python `filecollection/` + groupers. Close.

#### `dirtoo-filter`

**Role.** DSL parser, predicates (name, glob, regex, size, type, media dims/duration/fps, fuzzy, date/time/weekday, contains*, pages, filecount, random, charset), recursive search, MediaMetaCache (SQLite + ffprobe workers).

**Issues.**
- `type:video|image|archive|audio` now extension-regex (parity with Python).
- Content matchers read up to ~1MiB — worker only.
- `std::regex` ECMAScript quirks vs Python `re`.
- Media cache negative entries — important so paint does not spin.

**Parity.** Broadly aligned with Python `filter/` + `match_func_factory`. Python `find/` CLI language not fully ported (OOS).

#### `dirtoo-archive`

**Role.** `list_archive_entries` (bsdtar/tar/unzip), verbose size parsers, `fileinfos_for_prefix`, `extract_member`, `ArchiveManager` full extract to hashed cache dir with marker file.

**Issues.**
- Full extract for browse can be large; marker `.dirtoo-extracted` skips re-extract if mtime stamp in dir name matches.
- No write/update of archives (OOS).
- Password-protected archives fail at tool level — error surfaced via manager signals.

#### `dirtoo-watcher`

**Role.** `QFileSystemWatcher` wrapper (`DirectoryWatcher::Impl`). `start(Location)` watches the path string; `directoryChanged` → `directory_changed` signal. `stop()` removes paths.

**Issues.**
- **No fileChanged subscription** — only directory events; attribute-only updates may be missed on some backends.
- **No event payload** (which name changed) → MainWindow always soft-reloads full listing (merge_items).
- Coalesced in MainWindow (200ms timer). Comment in source notes open_location may still schedule an extra soft reload — possible double-fetch.
- Archive extract trees: watching the extracted cache dir is MainWindow’s responsibility when browsing archives.

#### `dirtoo-thumbnail`

**Role.** Freedesktop Thumbnailer1 D-Bus client + `cache_path_for` MD5 of file URL.

**Issues.**
- URL encoding gaps in Location affect cache keys (shared with fs).
- Failures for non-thumbnailable types should not sticky-error in UI (handled in MainWindow).

---

### `dirtoo/tools/`

| Tool | Notes |
|------|-------|
| `dt_copy/move/rename/rm/mkdir/mkfile/symlink/swap` | dirops frontends; expanded `--help`; `dt-move` requires `-t` |
| `dt_rmdir` | Empty-only or recursive empty-tree; does not delete files |
| `dt_mediainfo` | JSON-ish via media probe |
| `dt_archiveinfo` | Lists archive index |
| `cli_common.hpp` / `json_util.hpp` | Shared flags / minimal JSON escape |

**Parity.** Subset of Python `programs/`. Remaining Python utilities OOS.

---

### `dirtoo/tests/`

| Test | Covers |
|------|--------|
| `test_location` | URL/path/archive Location |
| `test_filter` | DSL predicates |
| `test_collection` | Hidden, glob, natural sort, group day |
| `test_dirops` / `test_dirops_rename` | Mutations + conflict rename |
| `test_archive_index` | `fileinfos_for_prefix` children |
| `test_clipboard_text` | copy/cut/link + GNOME parse |

**Gaps.** No size-from-verbose-listing test; no type:video test; no watcher/thumbnail/GUI.

---

### Cross-cutting defect themes

1. **Location URL encoding** — only space; affects thumbs + bookmarks with odd paths.
2. **MainWindow size** — continue extracting controllers (devices, archive session, open-with menus).
3. **Archive size path** — depends on external CLI verbose formats; parsers are best-effort.
4. **Search scale** — synthetic + batched refresh helps; true incremental model inserts would be better.
5. **Thumbnail fail semantics** — media-only error badges; good.
6. **No undo** — operations history is audit log only (intentional).

---

### Python tree (reference posture)

Do **not** extend `dirtoo-py/`. Use it to answer “what did the user expect?” Files under `programs/`, `expr/`, experiments remain mostly OOS unless they define filter/FS semantics already ported.

High-value Python reads for future parity questions:
- `fileview/controller.py` — selection info, paste, drop
- `fileview/file_item_renderer.py` — icon/montage paint
- `filter/match_func_factory.py` — full DSL surface
- `filesystem/location.py` — multi-payload / search URLs
- `gui/location_buttonbar.py` — breadcrumb UX

---


### Python reference notes

#### `dirtoo-py/src/dirtoo/filesystem/` (2026-08-02)

| File | Role | C++ counterpart |
|------|------|-----------------|
| `location.py` | Multi-payload Location, search protocol, URL parse | `dirtoo-fs` Location (subset) |
| `file_info.py` | Eager-ish file metadata | `FileInfo` |
| `lazy_file_info.py` | Lazy stat / access | Partial: cheap `from_directory_entry` |
| `stdio_filesystem.py` | FS + archive manager facade | Split: archive lib + app |
| `__init__.py` | Re-exports | n/a |

**Python-only capabilities to track in parity matrix**
- Nested/stacked archive payloads in one Location
- `search://` location type
- LazyFileInfo deferred stat and metadata bag
- StdioFilesystem as single entry for watchers + extractors

#### `dirtoo-py/src/dirtoo/posix/` (2026-08-02)

| File | Role | C++ |
|------|------|-----|
| `filesystem.py` | Non-destructive-by-default FS ops, progress callbacks | `dirops` |
| `__init__.py` | Package | n/a |

Python `Filesystem` class is a broad low-level toolkit (listdir, scandir, copy with buffer, etc.). C++ splits **read** (`dirtoo-fs`) from **mutate** (`dirops`). CLI merge/move semantics live partly in Python `programs/move.py` — compare when auditing `tools/dt_move.cpp`.


#### `dirtoo-py` filter / expr / find (2026-08-02)

| Package | Role | C++ |
|---------|------|-----|
| `filter/` | DSL parsers + MatchFunc factory + match funcs | `dirtoo-filter` |
| `expr/` | Expression AST helpers | folded into parser |
| `find/` | Walk + action context | `search_directory` (match only) |

**Python-only / deferred**
- Find **actions** (delete, exec, print beyond dt-filter)
- `pick:` match (disabled in Python too)
- Separate command vs expr parser modules — C++ single recursive descent

**Parity assessment:** Command set is largely **complete** for GUI filtering and recursive search. Media + content filters are the heavy pieces and are present.




---


#### `dirtoo-py` filecollection / watcher / thumbnail / archive (2026-08-02)

| Package | C++ counterpart | Gap notes |
|---------|-----------------|-----------|
| `filecollection/` | `dirtoo-collection` | Close; pluggable sort key → enum |
| `watcher/` | `dirtoo-watcher` | C++ is QFileSystemWatcher only; no archive watcher / fine-grained inotify |
| `thumbnail/` | `dirtoo-thumbnail` + app directory worker | Archive thumbs weak on both sides of port |
| `archive/` | `dirtoo-archive` | Python multi-extractor classes; C++ external tools + index |



#### `dirtoo-py` fileview / gui / bookmark / history (2026-08-02)

| Package | Role | C++ home |
|---------|------|----------|
| `fileview/` | Window, controller, graphics scene, actions, workers | `apps/dirtoo` especially `main_window`, graphics view, workers |
| `gui/` | Dialogs, menus, location bar, messages | matching `*_dialog`, `location_button_bar`, etc. |
| `bookmark/` | Bookmarks store | `bookmarks.*` |
| `history/` | Location history helpers | embedded in MainWindow + settings |

**Python-only weight**
- `virtual_filesystem.py`, richer `layout*` / kinetic ideas
- `executor.py` / `application_actions.py` structure (logic factoring)
- Some transfer request/error dialog variants merged into C++ transfer/conflict dialogs


## Feature parity (from file audits)

Status: **present** / **partial** / **missing** / **OOS** (out of scope for the C++ port).

### Core browsing

| Feature | Status | Notes |
|---------|--------|-------|
| Local directory browse | present | `dirtoo-fs` + load worker |
| Detail / Icons / Small-icons views | present | Graphics + list models |
| Zoom / icon detail levels / crop thumbs | present | App settings |
| Natural name sort + size/mtime/ext | present | `Sorter` |
| Media-aware sort (w/h/duration/…) | present | Needs MediaMetaCache fill |
| Group by day / directory / duration | present | |
| Show hidden | present | |
| Filter DSL (name + commands) | present | Strong parity with Python factory |
| Content filters off UI thread | present | FilterWorker |
| Recursive search | present | SearchWorker; not `search://` Location |
| Path / breadcrumb navigation | present | |
| Location history + bookmarks | present | |
| Recently Opened (app + files) | present | C++ strengthened vs typical Python use |
| Sidebar directory tree | present | |
| Devices (UDisks) | present | Verify mount/eject completeness in UX |
| Directory watching | partial | QFileSystemWatcher only; soft rescan; no archive watcher |
| Freedesktop thumbnails | present | D-Bus + cache fallback |
| Directory collage thumbnails | present | `directory_thumbnail_worker` |
| Archive member thumbnails | partial | Weak; needs extract/special URI |
| Archive browse (read-only) | present | Index + extract cache |
| Archive write / create | OOS | |
| Nested multi-payload locations | missing | Python Location stacks; C++ single archive layer |
| `search://` location protocol | missing | Search is a mode, not a Location |
| Remote / virtual VFS | OOS | |

### Mutations & clipboard

| Feature | Status | Notes |
|---------|--------|-------|
| Copy / move / rename / delete | present | `dirops` + GUI transfer |
| Mkdir / mkfile / symlink / swap | present | dirops + dialogs / tools |
| Conflict policies | present | Fail/Overwrite/Rename/Skip + dialog |
| Clipboard cut/copy/paste | present | |
| Paste as link | present | |
| Drag and drop | present | Overlay + drop-to-dir |
| Cross-device move safety | partial | copy+delete; partial failure → duplicates |
| Undo | missing | Not implemented |
| Editable permissions | missing | Properties display-only |

### Open / MIME / metadata

| Feature | Status | Notes |
|---------|--------|-------|
| Open with default / desktop apps | present | `open_with` |
| Open with command | present | |
| Open history tracking | present | |
| Properties dialog | present | Display |
| Media metadata (ffprobe + SQLite) | present | Async cache |
| Size units SI/IEC | present | Preferences |

### CLI & tests

| Feature | Status | Notes |
|---------|--------|-------|
| dt-* mutation CLIs | present | |
| dt-filter / mediainfo / archiveinfo | present | |
| Full Python `programs/*` suite | OOS | Intentionally not ported |
| Unit tests (libs) | present | Catch2 core libs |
| GUI automated tests | missing | |

### Packaging / UX polish

| Feature | Status | Notes |
|---------|--------|-------|
| Nix flake multi-package | present | |
| Qt6 + qtsvg | present | |
| GNOME dialog button order | present | Proxy style |
| Theme via qt6ct | env | Not app-forced beyond button layout |
| Incomplete URL percent-encoding | issue | Location only handles `%20` |

### Highest-value remaining gaps (in-scope)

1. **Operations history log** (timestamped mutations; no rollback yet)
2. **Editable permissions** (optional)
3. **Richer directory watcher** (event types / archive extract dir)
4. **Archive member thumbnails**
5. **Location URL encoding** for non-ASCII / reserved chars
6. **Mount/eject** polish if incomplete in Devices UI
7. Slim **`MainWindow`** by extracting controllers

---

## Next step

**Done (first full pass):** Inventories marked; libs, GUI, tools, tests, packaging, and major Python packages reviewed; **Feature parity** matrix rewritten.

**Optional follow-ups:**

1. Deep-dive any single 🔍 file (e.g. line-level `main_window.cpp` sections)
2. Devices mount/eject UX verification
3. Track gap list items into `TODO.md`

