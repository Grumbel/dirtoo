# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


from typing import TYPE_CHECKING, Optional, Sequence

import logging

from pkg_resources import resource_filename
from PyQt6.QtCore import Qt, QPoint, QKeyCombination
from PyQt6.QtGui import QKeySequence, QIcon, QCursor, QMovie, QCloseEvent, QShortcut
from PyQt6.QtWidgets import (
    QMenu,
    QHBoxLayout,
    QPushButton,
    QToolButton,
    QFormLayout,
    QWidget,
    QLabel,
    QMainWindow,
    QSizePolicy,
    QVBoxLayout,
    QToolBar
)

from dirtoo.fileview.file_view import FileView
from dirtoo.gui.filter_line_edit import FilterLineEdit
from dirtoo.gui.history_menu import make_history_menu_entries
from dirtoo.gui.label import Label
from dirtoo.gui.location_buttonbar import LocationButtonBar
from dirtoo.gui.location_lineedit import LocationLineEdit
from dirtoo.gui.menu import Menu
from dirtoo.gui.message_area import MessageArea
from dirtoo.gui.search_line_edit import SearchLineEdit
from dirtoo.gui.tool_button import ToolButton
from dirtoo.filesystem.location import Location
from dirtoo.image.icon import load_icon

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller


logger = logging.getLogger(__name__)


class FileViewWindow(QMainWindow):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self._message_area: Optional[MessageArea] = None

        self.controller = controller
        self._actions = self.controller.actions

        self.make_window()
        self.make_menubar()
        self.make_toolbar()
        self.addToolBarBreak()
        self.make_location_toolbar()
        self.make_filter_toolbar()
        self.make_search_toolbar()
        self.make_shortcut()

        self.file_view.setFocus()

        self.resize(1024, 768)
        self.move(QCursor.pos().x() - self.width() // 2,
                  QCursor.pos().y() - self.height() // 2)

        self.addAction(self._actions.rename)

    def __del__(self) -> None:
        logger.debug("FileViewWindow.__del__")

    def closeEvent(self, ev: QCloseEvent) -> None:
        self.controller.on_exit()

    def make_shortcut(self) -> None:
        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.CTRL, Qt.Key.Key_L)), self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(self.controller.show_location_toolbar)

        def show_filter() -> None:
            self.file_filter.setFocus(Qt.FocusReason.ShortcutFocusReason)
            self.filter_toolbar.show()

        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.CTRL, Qt.Key.Key_K)), self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(show_filter)

        shortcut = QShortcut(QKeySequence(Qt.Key.Key_F3), self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(self.controller.show_search)

        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.CTRL, Qt.Key.Key_F)), self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(self.controller.show_search)

        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.CTRL, Qt.Key.Key_W)), self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(self.controller.close_window)

        shortcut = QShortcut(QKeySequence(QKeyCombination(Qt.Modifier.ALT, Qt.Key.Key_Up)), self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(self.controller.parent_directory)

        shortcut = QShortcut(Qt.Key.Key_Home, self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.scroll_top())

        shortcut = QShortcut(Qt.Key.Key_End, self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.scroll_bottom())

        shortcut = QShortcut(Qt.Key.Key_PageUp, self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.scroll_by(0, -self.file_view.viewport().height()))

        shortcut = QShortcut(Qt.Key.Key_PageDown, self)
        shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.scroll_by(0, self.file_view.viewport().height()))

    def make_window(self) -> None:
        self.setWindowTitle("dirtoo")
        self.setWindowIcon(QIcon(resource_filename("dirtoo", "icons/dirtoo.png")))
        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = FileView(self.controller)

        self.search_lineedit = SearchLineEdit(self.controller)
        self.location_lineedit = LocationLineEdit(self.controller)
        self.location_buttonbar = LocationButtonBar(self.controller)
        self.file_filter = FilterLineEdit(self.controller)
        # self.file_filter.setText("File Pattern Here")
        self.file_view.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.status_bar = self.statusBar()

        self.info = QLabel("")
        self.status_bar.addPermanentWidget(self.info)

        self.vbox.addWidget(self.file_view, Qt.AlignmentFlag.AlignLeft)

        self._message_area = MessageArea()
        self._message_area.hide()
        self.vbox.addWidget(self._message_area)

        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

    def hide_filter(self) -> None:
        self.filter_toolbar.hide()

    def make_filter_toolbar(self) -> None:
        hbox = QHBoxLayout()

        form = QFormLayout()
        label = QLabel("Filter:")
        label.setBuddy(self.file_filter)
        form.addRow(label, self.file_filter)
        form.setContentsMargins(0, 0, 0, 0)
        hbox.addLayout(form)

        help_button = QPushButton("Help")
        help_button.clicked.connect(self.controller.show_filter_help)
        hbox.addWidget(help_button)
        hbox.setContentsMargins(0, 0, 0, 0)

        widget = QWidget()
        widget.setLayout(hbox)

        self.filter_toolbar = QToolBar("Filter")
        self.filter_toolbar.addWidget(widget)
        self.addToolBar(Qt.ToolBarArea.BottomToolBarArea, self.filter_toolbar)
        self.filter_toolbar.hide()

    def make_search_toolbar(self) -> None:
        hbox = QHBoxLayout()

        form = QFormLayout()
        label = QLabel("Search:")
        label.setBuddy(self.search_lineedit)
        form.addRow(label, self.search_lineedit)
        form.setContentsMargins(0, 0, 0, 0)
        hbox.addLayout(form)

        help_button = QPushButton("Help")
        help_button.clicked.connect(self.controller.show_search_help)
        hbox.addWidget(help_button)
        hbox.setContentsMargins(0, 0, 0, 0)

        widget = QWidget()
        widget.setLayout(hbox)

        self.search_toolbar = QToolBar("Search")
        self.search_toolbar.addWidget(widget)
        self.addToolBar(Qt.ToolBarArea.TopToolBarArea, self.search_toolbar)
        self.search_toolbar.hide()

    def make_location_toolbar(self) -> None:
        self.location_toolbar = self.addToolBar("Location")
        widget = QWidget()
        form = QFormLayout()
        label = Label("Location:")
        label.clicked.connect(self.controller.show_location_toolbar)

        def show_location_menu(pos: QPoint) -> None:
            self.controller.on_context_menu(label.mapToGlobal(pos))

        label.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        label.customContextMenuRequested.connect(show_location_menu)

        both = QVBoxLayout()
        both.addWidget(self.location_buttonbar)
        both.addWidget(self.location_lineedit)
        form.addRow(label, both)

        form.setContentsMargins(0, 0, 0, 0)
        widget.setLayout(form)
        self.location_toolbar.addWidget(widget)

        self.addToolBarBreak(Qt.ToolBarArea.TopToolBarArea)

    def make_group_menu(self) -> QMenu:
        menu = QMenu("Group Options")
        menu.addAction(self._actions.group_by_none)
        menu.addAction(self._actions.group_by_day)
        menu.addAction(self._actions.group_by_directory)
        menu.addAction(self._actions.group_by_duration)
        return menu

    def make_sort_menu(self) -> QMenu:
        menu = QMenu("Sort Options")
        menu.addSeparator().setText("Sort Options")
        menu.addAction(self._actions.sort_directories_first)
        menu.addAction(self._actions.sort_reversed)
        menu.addSeparator().setText("Sort by")
        menu.addAction(self._actions.sort_by_name)
        menu.addAction(self._actions.sort_by_size)
        menu.addAction(self._actions.sort_by_ext)
        menu.addAction(self._actions.sort_by_date)
        menu.addAction(self._actions.sort_by_duration)
        menu.addAction(self._actions.sort_by_aspect_ratio)
        menu.addAction(self._actions.sort_by_framerate)
        menu.addAction(self._actions.sort_by_resolution)
        menu.addAction(self._actions.sort_by_user)
        menu.addAction(self._actions.sort_by_group)
        menu.addAction(self._actions.sort_by_permission)
        menu.addAction(self._actions.sort_by_random)
        return menu

    def make_view_menu(self) -> QMenu:
        menu = QMenu("View Options")
        menu.addSeparator().setText("View Options")
        menu.addAction(self._actions.show_abspath)
        menu.addAction(self._actions.show_basename)
        menu.addSeparator()
        menu.addAction(self._actions.toggle_timegaps)
        return menu

    def make_menubar(self) -> None:
        self.menubar = self.menuBar()
        file_menu = self.menubar.addMenu('&File')
        file_menu.addAction(self._actions.new_window)
        file_menu.addAction(self._actions.parent_directory)
        file_menu.addAction(self._actions.debug)
        file_menu.addAction(self._actions.save_as)
        file_menu.addSeparator()
        file_menu.addAction(self._actions.exit)

        edit_menu = self.menubar.addMenu('&Edit')
        # edit_menu.addAction(self._actions.undo)
        # edit_menu.addAction(self._actions.redo)
        # edit_menu.addSeparator()
        edit_menu.addAction(self._actions.edit_cut)
        edit_menu.addAction(self._actions.edit_copy)
        edit_menu.addAction(self._actions.edit_paste)
        edit_menu.addSeparator()
        edit_menu.addAction(self._actions.edit_select_all)
        edit_menu.addSeparator()
        edit_menu.addAction(self._actions.show_preferences)

        view_menu = self.menubar.addMenu('&View')
        view_menu.addSeparator().setText("View Style")
        view_menu.addAction(self._actions.view_detail_view)
        view_menu.addAction(self._actions.view_icon_view)
        view_menu.addAction(self._actions.view_small_icon_view)
        view_menu.addSeparator().setText("Filter")
        view_menu.addAction(self._actions.show_hidden)
        view_menu.addAction(self._actions.show_filtered)
        view_menu.addSeparator().setText("Path Options")
        view_menu.addAction(self._actions.show_abspath)
        view_menu.addAction(self._actions.show_basename)
        view_menu.addSeparator().setText("Sort Options")
        view_menu.addAction(self._actions.sort_directories_first)
        view_menu.addAction(self._actions.sort_reversed)
        view_menu.addAction(self._actions.sort_by_name)
        view_menu.addAction(self._actions.sort_by_size)
        view_menu.addAction(self._actions.sort_by_ext)
        view_menu.addAction(self._actions.sort_by_duration)
        view_menu.addAction(self._actions.sort_by_aspect_ratio)
        view_menu.addAction(self._actions.sort_by_framerate)
        view_menu.addAction(self._actions.sort_by_resolution)
        view_menu.addAction(self._actions.sort_by_user)
        view_menu.addAction(self._actions.sort_by_group)
        view_menu.addAction(self._actions.sort_by_permission)
        view_menu.addAction(self._actions.sort_by_random)
        view_menu.addSeparator().setText("Zoom")
        view_menu.addAction(self._actions.zoom_in)
        view_menu.addAction(self._actions.zoom_out)
        view_menu.addSeparator()

        self.bookmarks_menu = Menu('&Bookmarks')
        self.menubar.addMenu(self.bookmarks_menu)

        def create_bookmarks_menu() -> None:
            bookmarks = self.controller.app.bookmarks

            entries: Sequence[Location] = bookmarks.get_entries()

            self.bookmarks_menu.clear()

            if self.controller.location is None:
                action = self.bookmarks_menu.addAction(load_icon("user-bookmarks"), "Can't bookmark file lists")
                action.setEnabled(False)
            elif self.controller.location in entries:
                self.bookmarks_menu.addAction(
                    load_icon("edit-delete"), "Remove This Location's Bookmark",
                    lambda loc=self.controller.location:
                    bookmarks.remove(loc))
            else:
                self.bookmarks_menu.addAction(
                    load_icon("bookmark-new"), "Bookmark This Location",
                    lambda loc=self.controller.location:
                    bookmarks.append(loc))
            self.bookmarks_menu.addSeparator()

            self.bookmarks_menu.addDoubleAction(
                load_icon("folder"), "View Bookmarks",
                lambda: self.controller.set_location(Location("bookmarks", "/", [])),
                lambda: self.controller.new_controller().set_location(Location("bookmarks", "/", [])))

            self.bookmarks_menu.addSeparator()

            icon = load_icon("folder")
            for entry in entries:
                action = self.bookmarks_menu.addDoubleAction(
                    icon, entry.as_url(),
                    lambda entry=entry: self.controller.set_location(entry),  # type: ignore
                    lambda entry=entry: self.controller.app.show_location(entry))  # type: ignore

                # FIXME: exists() checks must be done asynchronously
                if False and not entry.exists():  # type: ignore  # pylint: disable=R1727
                    action.setEnabled(False)  # type: ignore
        self.bookmarks_menu.aboutToShow.connect(create_bookmarks_menu)

        self.history_menu = Menu('&History')
        self.menubar.addMenu(self.history_menu)

        def create_history_menu() -> None:
            history = self.controller.app.location_history

            self.history_menu.clear()
            self.history_menu.addAction(self._actions.back)
            self.history_menu.addAction(self._actions.forward)
            self.history_menu.addSeparator()

            self.history_menu.addDoubleAction(
                load_icon("folder"), "View File History",
                self.controller.show_file_history,
                lambda: self.controller.new_controller().show_file_history())

            self.history_menu.addSection("Location History")

            entries = history.get_unique_entries(35)
            make_history_menu_entries(self.controller, self.history_menu, entries)

        self.history_menu.aboutToShow.connect(create_history_menu)

        help_menu = self.menubar.addMenu('&Help')
        help_menu.addAction(self._actions.about)

    def make_toolbar(self) -> None:
        self.toolbar = self.addToolBar("FileView")

        tool_button = ToolButton()
        tool_button.setDefaultAction(self._actions.parent_directory)
        tool_button.sig_middle_click.connect(lambda: self.controller.parent_directory(new_window=True))
        tool_button.setIcon(self._actions.parent_directory.icon())
        self.toolbar.addWidget(tool_button)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.home)
        self.toolbar.addSeparator()

        history_back_btn = ToolButton()
        history_back_btn.setDefaultAction(self._actions.back)
        history_back_btn.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        history_back_btn.customContextMenuRequested.connect(
            lambda pos: self.controller.show_history_context_menu(history_back_btn, False))
        self.toolbar.addWidget(history_back_btn)

        history_forward_btn = ToolButton()
        history_forward_btn.setDefaultAction(self._actions.forward)
        history_forward_btn.setContextMenuPolicy(Qt.ContextMenuPolicy.CustomContextMenu)
        history_forward_btn.customContextMenuRequested.connect(
            lambda pos: self.controller.show_history_context_menu(history_forward_btn, False))
        self.toolbar.addWidget(history_forward_btn)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.reload)
        self.toolbar.addAction(self._actions.prepare)
        # self.toolbar.addSeparator()
        # self.toolbar.addAction(self._actions.undo)
        # self.toolbar.addAction(self._actions.redo)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.show_hidden)

        button = QToolButton()
        button.setIcon(load_icon("view-restore"))
        button.setMenu(self.make_view_menu())
        button.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        self.toolbar.addWidget(button)

        button = QToolButton()
        button.setIcon(load_icon("view-sort-ascending"))
        button.setMenu(self.make_sort_menu())
        button.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        self.toolbar.addWidget(button)

        button = QToolButton()
        button.setIcon(load_icon("view-sort-ascending"))
        button.setMenu(self.make_group_menu())
        button.setPopupMode(QToolButton.ToolButtonPopupMode.InstantPopup)
        self.toolbar.addWidget(button)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.view_icon_view)
        self.toolbar.addAction(self._actions.view_small_icon_view)
        self.toolbar.addAction(self._actions.view_detail_view)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.zoom_in)
        self.toolbar.addAction(self._actions.zoom_out)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.lod_in)
        self.toolbar.addAction(self._actions.lod_out)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self._actions.crop_thumbnails)

        # Spacer to force right alignment for all following widget
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        self.toolbar.addWidget(spacer)

        self.toolbar.addAction(self._actions.debug)
        self.toolbar.addAction(self.controller.app.actions.enable_filesystem)

        # Loading icon
        self.loading_movie = QMovie(resource_filename("dirtoo", "icons/gears.gif"))
        self.loading_label = QLabel()
        self.toolbar.addWidget(self.loading_label)

    def show_loading(self) -> None:
        self.show_info("Loading...")
        self.loading_label.setMovie(self.loading_movie)
        self.loading_movie.start()
        self.loading_label.show()

    def hide_loading(self) -> None:
        self.show_info("")
        self.loading_movie.stop()
        self.loading_label.clear()
        self.loading_label.setVisible(False)

    def zoom_in(self) -> None:
        self.file_view.zoom_in()

    def zoom_out(self) -> None:
        self.file_view.zoom_out()

    def set_location(self, location: Location) -> None:
        self.location_lineedit.set_location(location)
        self.location_buttonbar.set_location(location)
        self.setWindowTitle("{} - dirtoo".format(location.as_human()))

    def set_file_list(self) -> None:
        self.location_lineedit.set_unused_text()

    def show_info(self, text: str) -> None:
        self.info.setText("  " + text)

    def show_current_filename(self, filename: str) -> None:
        # FIXME: this causes quite substantial keyboard lag when
        # scrolling with PageUp/Down
        self.status_bar.showMessage(filename)


# EOF #
