// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Shared includes for MainWindow multi-TU implementations.
// main_window.hpp only forward-declares many Qt types; .cpp units that
// dereference members must include complete definitions (via this header).

#include "main_window.hpp"

#include "file_list_model.hpp"
#include "graphics_file_view.hpp"
#include "graphics_file_item.hpp"
#include "directory_tree_model.hpp"
#include "leap_widget.hpp"
#include "location_button_bar.hpp"
#include "message_area.hpp"
#include "path_completion_worker.hpp"
#include "filter_worker.hpp"
#include "sort_worker.hpp"
#include "directory_load_worker.hpp"
#include "directory_thumbnail_worker.hpp"
#include "theme_icons.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCompleter>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QShowEvent>
#include <QSplitter>
#include <QStringListModel>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QHeaderView>
#include <QKeySequence>
#include <QPushButton>
#include <QFrame>
#include <QScrollBar>

