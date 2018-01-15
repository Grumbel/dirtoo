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


import os

from pkg_resources import resource_filename
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QKeySequence, QIcon, QPalette
from PyQt5.QtWidgets import (
    QFormLayout,
    QStyle,
    QWidget,
    QLabel,
    QLineEdit,
    QMainWindow,
    QSizePolicy,
    QShortcut,
    QVBoxLayout,
)

from dirtools.fileview.detail_view import DetailView
from dirtools.fileview.thumb_view import ThumbView


class FileFilter(QLineEdit):

    def __init__(self, controller):
        super().__init__()
        self.controller = controller
        self.is_unused = True
        self.set_unused_text()
        self.returnPressed.connect(self.on_return_pressed)

    def on_return_pressed(self):
        self.controller.set_filter(self.text())

    def focusInEvent(self, ev):
        super().focusInEvent(ev)

        if self.is_unused:
            self.setText("")

        p = self.palette()
        p.setColor(QPalette.Text, Qt.black)
        self.setPalette(p)

    def focusOutEvent(self, ev):
        super().focusOutEvent(ev)

        if self.text() == "":
            self.is_unused = True
            self.set_unused_text()
        else:
            self.is_unused = False

    def set_unused_text(self):
        p = self.palette()
        p.setColor(QPalette.Text, Qt.gray)
        self.setPalette(p)
        self.setText("enter a glob search pattern here")


class FilePathLineEdit(QLineEdit):

    def __init__(self, controller):
        super().__init__()
        self.controller = controller
        self.returnPressed.connect(self.onReturnPressed)

    def onReturnPressed(self):
        if os.path.exists(self.text()):
            self.controller.set_location(self.text())


class FileViewWindow(QMainWindow):

    def __init__(self, controller):
        super().__init__()

        self.controller = controller
        self.actions = self.controller.actions

        self.make_window()
        self.make_menubar()
        self.make_toolbar()
        self.make_shortcut()

    def make_shortcut(self):
        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_L), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_path.setFocus(Qt.ShortcutFocusReason))

        shortcut = QShortcut(QKeySequence(Qt.CTRL + Qt.Key_K), self)
        shortcut.setContext(Qt.WindowShortcut)
        shortcut.activated.connect(lambda: self.file_filter.setFocus(Qt.ShortcutFocusReason))

    def make_window(self):
        self.setWindowTitle("dt-fileview")
        self.setWindowIcon(QIcon(resource_filename("dirtools", "fileview/fileview.svg")))
        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = DetailView(self.controller)
        self.file_view.hide()
        self.thumb_view = ThumbView(self.controller)
        self.file_path = FilePathLineEdit(self.controller)
        self.file_filter = FileFilter(self.controller)
        # self.file_filter.setText("File Pattern Here")
        self.file_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.thumb_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.status_bar = self.statusBar()

        form = QFormLayout()
        label = QLabel("Location:")
        label.setBuddy(self.file_path)
        form.addRow(label, self.file_path)
        self.vbox.addLayout(form)

        self.vbox.addWidget(self.file_view, Qt.AlignLeft)
        self.vbox.addWidget(self.thumb_view, Qt.AlignLeft)

        QKeySequence("Ctrl+f")
        form = QFormLayout()
        label = QLabel("Filter:")
        label.setBuddy(self.file_filter)
        form.addRow(label, self.file_filter)
        self.vbox.addLayout(form)

        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

    def make_menubar(self):
        self.menubar = self.menuBar()
        file_menu = self.menubar.addMenu('&File')
        file_menu.addAction(self.actions.save_as)
        file_menu.addSeparator()
        file_menu.addAction(self.actions.exit)

        edit_menu = self.menubar.addMenu('&Edit')
        edit_menu.addAction(self.actions.undo)
        edit_menu.addAction(self.actions.redo)

        view_menu = self.menubar.addMenu('&View')
        view_menu.addSeparator().setText("View Style")
        view_menu.addAction(self.actions.view_detail_view)
        view_menu.addAction(self.actions.view_icon_view)
        view_menu.addAction(self.actions.view_small_icon_view)
        view_menu.addSeparator().setText("Filter")
        view_menu.addAction(self.actions.show_hidden)
        view_menu.addSeparator().setText("Path Options")
        view_menu.addAction(self.actions.show_abspath)
        view_menu.addAction(self.actions.show_basename)
        view_menu.addSeparator().setText("Zoom")
        view_menu.addAction(self.actions.zoom_in)
        view_menu.addAction(self.actions.zoom_out)
        view_menu.addSeparator()

        history_menu = self.menubar.addMenu('&History')
        history_menu.addAction(self.actions.back)
        history_menu.addAction(self.actions.forward)

        help_menu = self.menubar.addMenu('&Help')
        help_menu.addAction(self.actions.about)

    def make_toolbar(self):
        self.toolbar = self.addToolBar("FileView")
        self.toolbar.addAction(self.style().standardIcon(QStyle.SP_FileDialogToParent),
                               "Parent Directory",
                               self.controller.parent_directory)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.back)
        self.toolbar.addAction(self.actions.forward)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.reload)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.undo)
        self.toolbar.addAction(self.actions.redo)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.show_hidden)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.show_abspath)
        self.toolbar.addAction(self.actions.show_basename)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.toggle_timegaps)
        self.toolbar.addSeparator()
        # self.toolbar.addAction(self.style().standardIcon(QStyle.SP_FileDialogContentsView),
        #                       "List View", self.show_list_view)
        self.toolbar.addAction(self.style().standardIcon(QStyle.SP_FileDialogContentsView),
                               "Thumbnail View",
                               self.show_thumb_view)
        self.toolbar.addAction(self.style().standardIcon(QStyle.SP_FileDialogDetailedView),
                               "Detail View",
                               self.show_detail_view)
        self.toolbar.addSeparator()
        self.toolbar.addAction(self.actions.zoom_in)
        self.toolbar.addAction(self.actions.zoom_out)
        self.toolbar.addSeparator()
        self.info = QLabel("lots of files selected")
        self.toolbar.addWidget(self.info)

    def zoom_in(self):
        self.thumb_view.zoom_in()

    def zoom_out(self):
        self.thumb_view.zoom_out()

    def show_detail_view(self):
        self.thumb_view.hide()
        self.file_view.show()

    def show_thumb_view(self):
        self.thumb_view.show()
        self.file_view.hide()

    def set_location(self, path):
        self.file_path.setText(path)

    def show_info(self, text):
        self.info.setText("  " + text)

    def show_current_filename(self, filename):
        self.status_bar.showMessage(filename)


# EOF #
