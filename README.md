# dirtoo

Qt-based file manager. This repository contains:

| Path | Role |
|------|------|
| **`dirtoo/`** | Active **C++23 / Qt6** implementation |
| **`dirtoo-py/`** | Frozen **Python / PyQt6** behavioral reference |
| **`AGENTS.md`** | Rules for contributors and coding agents |
| **`TODO.md`** | Port checklist, residual work, operations-history plan |
| **`AUDIT.md`** | Source inventory and feature-parity audit (living) |

Start with **[`dirtoo/README.md`](dirtoo/README.md)** for build instructions,
features, and layout.

### Port status (summary)

Local GUI MVP is in place: navigation, filter DSL, search, three view modes,
thumbnails, clipboard/DnD/transfers, read-only archives, bookmarks, location
history, and **Recently Opened** (file open history).

**Not in scope / deferred:** archive write, remote VFS, full Python `programs/*`,
kinetic layouts. **No full Undo** — planned instead is an **operations history
log** (timestamped rename/move/copy/delete/…); rollback is deferred.

License: **GPL-3.0-or-later**.
