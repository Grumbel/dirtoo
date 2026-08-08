# dirtoo

**dirtoo** is a Qt-based **local file manager** aimed at media-heavy folders:
filtering, thumbnails, archives (read-only), and keyboard-friendly browsing.

| Path | Role |
|------|------|
| **[`dirtoo/`](dirtoo/)** | Active **C++23 / Qt6** app — start with **[`dirtoo/README.md`](dirtoo/README.md)** for features, shortcuts, and build |
| **`dirtoo-py/`** | Frozen **Python / PyQt6** behavioral reference |
| **`AGENTS.md`** | Rules for contributors and coding agents |
| **`TODO.md`** | Residual work and port checklist |
| **`AUDIT.md`** | Source inventory and parity notes |

### Status (short)

The local GUI covers navigation, filter DSL, recursive search, several view
modes, thumbnails, clipboard and drag-and-drop transfers, read-only archives,
bookmarks, history, and devices. It is still under development — see the
startup warning in the app.

**Out of scope for now:** archive write, remote VFS, full undo.

License: **GPL-3.0-or-later**.
