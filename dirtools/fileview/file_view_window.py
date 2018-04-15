# dirtool.py - diff tool for directories
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


from typing import Optional

from pkg_resources import resource_filename
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QKeySequence, QIcon, QCursor, QMovie
from PyQt5.QtWidgets import (
    QMenu,
    QHBoxLayout,
    QPushButton,
    QToolButton,
    QFormLayout,
    QWidget,
    QLabel,
    QMainWindow,
    QSizePolicy,
    QShortcut,
    QVBoxLayout,
    QToolBar
)

from dirtools.fileview.file_view import FileView
from dirtools.fileview.location import Location
from dirtools.fileview.menu import Menu
from dirtools.fileview.tool_button import ToolButton
from dirtools.fileview.message_area import MessageArea
from dirtools.fileview.history_menu import make_history_menu_entries

if False:
    from dirtools.fileview.controller import Controller  # noqa: F401


class FileViewWindow(QMainWindow):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self._message_area: Optional[MessageArea] = None

        self.controller = controller
        self.actions = self.controller.actions

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
        self.move(QCursor.pos().x() - self.width() / 2,
                  QCursor.pos().y() - self.height() / 2)

        self.addAction(self.actions.rename)

    def closeEvent(self, ev):
        self.controller.on_exit()

    def make_shortcut(self):
        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_L), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(self.controller.show_location_toolbar)

        def show_filter():
            self.file_filter.setFocus(Qt.ShortcutFocusReason)
            self.filter_toolbar.show()

        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_K), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(show_filter)

        shortcut = QShortcut(QKeySequence(Qt.Key_F3), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(self.controller.show_search)

        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_F), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(self.controller.show_search)

        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_W), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(self.controller.close_window)

        shortcut = QShortcut(QKeySequence(Qt.ALT + Qt.Key_Up), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(self.controller.parent_directory)

        shortcut = QShortcut(Qt.Key_Home, self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.ensureVisible(0, 0, 1, 1))

        shortcut = QShortcut(Qt.Key_End, self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(
            lambda: self.file_view.ensureVisible(
                0, self.file_view._layout.get_bounding_rect().height(), 1, 1))

        shortcut = QShortcut(Qt.Key_PageUp, self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.scroll_by(0, -self.file_view.viewport().height()))

        shortcut = QShortcut(Qt.Key_PageDown, self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_view.scroll_by(0, self.file_view.viewport().height()))

    def make_window(self):
        from dirtools.fileview.location_lineedit import LocationLineEdit
        from dirtools.fileview.location_buttonbar import LocationButtonBar
        from dirtools.fileview.filter_line_edit import FilterLineEdit
        from dirtools.fileview.search_line_edit import SearchLineEdit

        self.setWindowTitle("dt-fileview")
        self.setWindowIcon(QIcon(resource_filename("dirtools", "fileview/dt-fileview.svg")))
        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = FileView(self.controller)

        self.search_lineedit = SearchLineEdit(self.controller)
        self.location_lineedit = LocationLineEdit(self.controller)
        self.location_buttonbar = LocationButtonBar(self.controller)
        self.file_filter = FilterLineEdit(self.controller)
        # self.file_filter.setText("File Pattern Here")
        self.file_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.status_bar = self.statusBar()

        self.info = QLabel("")
        self.status_bar.addPermanentWidget(self.info)

        self.vbox.addWidget(self.file_view, Qt.AlignLeft)

        self._message_area = MessageArea()
        self._message_area.hide()
        self.vbox.addWidget(self._message_area)

        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

    def hide_filter(self):
        self.filter_toolbar.hide()

    def make_filter_toolbar(self):
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
        self.addToolBar(Qt.BottomToolBarArea, self.filter_toolbar)
        self.filter_toolbar.hide()

    def make_search_toolbar(self):
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
        self.addToolBar(Qt.TopToolBarArea, self.search_toolbar)
        self.search_toolbar.hide()

    def make_location_toolbar(self):
        self.location_toolbar = self.addToolBar("Location")
        widget = QWidget()
        form = QFormLayout()
        label = QLabel("Location:")
        label2 = QLabel("Location:")

        def show_location_menu(pos):
            self.controller.on_context_menu(label.mapToGlobal(pos))

        label.setContextMenuPolicy(Qt.CustomContextMenu)
        label.customContextMenuRequested.connect(show_location_menu)

        label.setBuddy(self.location_lineedit)
        form.addRow(label, self.location_lineedit)
        form.addRow(label2, self.location_buttonbar)

        form.setContentsMargins(0, 0, 0, 0)
        widget.setLayout(form)
        self.location_toolbar.addWidget(widget)

        self.addToolBarBreak(Qt.TopToolBarArea)

    def make_group_menu(self):
        menu = QMenu("Group Options")
        menu.addAction(self.actions.group_by_none)
        menu.addAction(self.actions.group_by_day)
        menu.addAction(self.actions.group_by_directory)
        menu.addAction(self.actions.group_by_duration)
        return menu

    def make_sort_menu(self):
        menu = QMenu("Sort Options")
        menu.addSeparator().setText("Sort Options")
        menu.addAction(self.actions.sort_directories_first)
        menu.addAction(self.actions.sort_reversed)
        menu.addSeparator().setText("Sort by")
        menu.addAction(self.actions.sort_by_name)
        menu.addAction(self.actions.sort_by_size)
        menu.addAction(self.actions.sort_by_ext)
        menu.addAction(self.actions.sort_by_date)
        menu.addAction(self.actions.sort_by_duration)
        menu.addAction(self.actions.sort_by_aspect_ratio)
        menu.addAction(self.actions.sort_by_framerate)
        menu.addAction(self.actions.sort_by_area)
        menu.addAction(self.actions.sort_by_user)
        menu.addAction(self.actions.sort_by_group)
        menu.addAction(self.actions.sort_by_permission)
        menu.addAction(self.actions.sort_by_random)
        return menu

    def make_view_menu(self):
        menu = QMenu("View Options")
        menu.addSeparator().setText("View Options")
        menu.addAction(self.actions.show_abspath)
        menu.addAction(self.actions.show_basename)
        menu.addSeparator()
        menu.addAction(self.actions.toggle_timegaps)
        return menu

    def make_menubar(self):
        self.menubar = self.menuBar()
        file_menu = self.menubar.addMenu('&File')
        file_menu.addAction(self.actions.new_window)
        file_menu.addAction(self.actions.parent_directory)
        file_menu.addAction(self.actions.debug)
        file_menu.addAction(self.actions.save_as)
        file_menu.addSeparator()
        file_menu.addAction(self.actions.exit)

        edit_menu = self.menubar.addMenu('&Edit')
        # edit_menu.addAction(self.actions.undo)
        # edit_menu.addAction(self.actions.redo)
        # edit_menu.addSeparator()
        edit_menu.addAction(self.actions.edit_cut)
        edit_menu.addAction(self.actions.edit_copy)
        edit_menu.addAction(self.actions.edit_paste)
        edit_menu.addSeparator()
        edit_menu.addAction(self.actions.edit_select_all)
        edit_menu.addSeparator()
        edit_menu.addAction(self.actions.show_preferences)

        view_menu = self.menubar.addMenu('&View')
        view_menu.addSeparator().setText("View Style")
        view_menu.addAction(self.actions.view_detail_view)
        view_menu.addAction(self.actions.view_icon_view)
        view_menu.addAction(self.actions.view_small_icon_view)
        view_menu.addSeparator().setText("Filter")
        view_menu.addAction(self.actions.show_hidden)
        view_menu.addAction(self.actions.show_filtered)
        view_menu.addSeparator().setText("Path Options")
        view_menu.addAction(self.actions.show_abspath)
        view_menu.addAction(self.actions.show_basename)
        view_menu.addSeparator().setText("Sort Options")
        view_menu.addAction(self.actions.sort_directories_first)
        view_menu.addAction(self.actions.sort_reversed)
        view_menu.addAction(self.actions.sort_by_name)
        view_menu.addAction(self.actions.sort_by_size)
        view_menu.addAction(self.actions.sort_by_ext)
        view_menu.addAction(self.actions.sort_by_duration)
        view_menu.addAction(self.actions.sort_by_aspect_ratio)
        view_menu.addAction(self.actions.sort_by_framerate)
        view_menu.addAction(self.actions.sort_by_area)
        view_menu.addAction(self.actions.sort_by_user)
        view_menu.addAction(self.actions.sort_by_group)
        view_menu.addAction(self.actions.sort_by_permission)
        view_menu.addAction(self.actions.sort_by_random)
        view_menu.addSeparator().setText("Zoom")
        view_menu.addAction(self.actions.zoom_in)
        view_menu.addAction(self.actions.zoom_out)
        view_menu.addSeparator()

        bookmarks_menu = Menu('&Bookmarks')
        self.menubar.addMenu(bookmarks_menu)

        def create_bookmarks_menu():
            bookmarks = self.controller.app.bookmarks

            entries = bookmarks.get_entries()

            bookmarks_menu.clear()

            if self.controller.location is None:
                action = bookmarks_menu.addAction(QIcon.fromTheme("user-bookmarks"), "Can't bookmark file lists")
                action.setEnabled(False)
            elif self.controller.location in entries:
                bookmarks_menu.addAction(QIcon.fromTheme("edit-delete"), "Remove This Location's Bookmark",
                                         lambda loc=self.controller.location:
                                         bookmarks.remove(loc))
            else:
                bookmarks_menu.addAction(QIcon.fromTheme("bookmark-new"), "Bookmark This Location",
                                         lambda loc=self.controller.location:
                                         bookmarks.append(loc))
            bookmarks_menu.addSeparator()

            icon = QIcon.fromTheme("folder")
            for entry in entries:
                action = bookmarks_menu.addDoubleAction(
                    icon, entry.as_url(),
                    lambda entry=entry: self.controller.set_location(entry),
                    lambda entry=entry: self.controller.app.show_location(entry))

                if not entry.exists():
                    action.setEnabled(False)
        bookmarks_menu.aboutToShow.connect(create_bookmarks_menu)

        history_menu = Menu('&History')
        self.menubar.addMenu(history_menu)

        def create_history_menu():
            history = self.controller.app.location_history

            history_menu.clear()
            history_menu.addAction(self.actions.back)
            history_menu.addAction(self.actions.forward)
            history_menu.addSeparator()

            history_menu.addDoubleAction(
                QIcon.fromTheme("folder"), "View File History",
                self.controller.show_file_history,
                lambda: self.controller.new_controller().show_file_history())

            history_menu.addSection("Location History")

            entries = history.get_unique_entries(35)
            make_history_menu_entries(self.controller, history_menu, entries)

        history_menu.aboutToShow.connect(create_history_menu)

        help_menu = self.menubar.addMenu('&Help')
        help_menu.addAction(self.actions.about)

    def make_toolbar(self):
        self.toolbar = self.addToolBar("FileView")

        button = ToolButton()
        button.setDefaultAction(self.actions.parent_directory)
        button.sig_middle_click.connect(lambda: self.controller.parent_directory(new_window=True))
        button.setIcon(self.actions.parent_directory.icon())
        self.toolbar.addWidget(button)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.home)
        self.toolbar.addSeparator()

        history_back_btn = ToolButton()
        history_back_btn.setDefaultAction(self.actions.back)
        history_back_btn.setContextMenuPolicy(Qt.CustomContextMenu)
        history_back_btn.customContextMenuRequested.connect(
            lambda pos: self.controller.show_history_context_menu(history_back_btn, False))
        self.toolbar.addWidget(history_back_btn)

        history_forward_btn = ToolButton()
        history_forward_btn.setDefaultAction(self.actions.forward)
        history_forward_btn.setContextMenuPolicy(Qt.CustomContextMenu)
        history_forward_btn.customContextMenuRequested.connect(
            lambda pos: self.controller.show_history_context_menu(history_forward_btn, False))
        self.toolbar.addWidget(history_forward_btn)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.reload)
        self.toolbar.addAction(self.actions.prepare)
        # self.toolbar.addSeparator()
        # self.toolbar.addAction(self.actions.undo)
        # self.toolbar.addAction(self.actions.redo)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.show_hidden)

        button = QToolButton()
        button.setIcon(QIcon.fromTheme("view-restore"))
        button.setMenu(self.make_view_menu())
        button.setPopupMode(QToolButton.InstantPopup)
        self.toolbar.addWidget(button)

        button = QToolButton()
        button.setIcon(QIcon.fromTheme("view-sort-ascending"))
        button.setMenu(self.make_sort_menu())
        button.setPopupMode(QToolButton.InstantPopup)
        self.toolbar.addWidget(button)

        button = QToolButton()
        button.setIcon(QIcon.fromTheme("view-sort-ascending"))
        button.setMenu(self.make_group_menu())
        button.setPopupMode(QToolButton.InstantPopup)
        self.toolbar.addWidget(button)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.view_icon_view)
        self.toolbar.addAction(self.actions.view_small_icon_view)
        self.toolbar.addAction(self.actions.view_detail_view)

        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.zoom_in)
        self.toolbar.addAction(self.actions.zoom_out)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.lod_in)
        self.toolbar.addAction(self.actions.lod_out)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.crop_thumbnails)

        # Spacer to force right alignment for all following widget
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.toolbar.addWidget(spacer)

        self.toolbar.addAction(self.actions.debug)
        self.toolbar.addAction(self.controller.app.actions.enable_filesystem)

        # Loading icon
        self.loading_movie = QMovie(resource_filename("dirtools", "fileview/icons/gears.gif"))
        self.loading_label = QLabel()
        self.toolbar.addWidget(self.loading_label)

    def show_loading(self):
        self.show_info("Loading...")
        self.loading_label.setMovie(self.loading_movie)
        self.loading_movie.start()
        self.loading_label.show()

    def hide_loading(self):
        self.show_info("")
        self.loading_movie.stop()
        self.loading_label.setMovie(None)
        self.loading_label.setVisible(False)

    def zoom_in(self):
        self.file_view.zoom_in()

    def zoom_out(self):
        self.file_view.zoom_out()

    def set_location(self, location: Location):
        self.location_lineedit.set_location(location)
        self.location_buttonbar.set_location(location)
        self.setWindowTitle("{} - dt-fileview".format(location.as_human()))

    def set_file_list(self):
        self.location_lineedit.set_unused_text()

    def show_info(self, text):
        self.info.setText("  " + text)

    def show_current_filename(self, filename):
        # FIXME: this causes quite substantial keyboard lag when
        # scrolling with PageUp/Down
        self.status_bar.showMessage(filename)


# EOF #
