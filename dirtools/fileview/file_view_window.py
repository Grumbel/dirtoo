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

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (
    QStyle,
    QAction,
    QActionGroup,
    QWidget,
    QLabel,
    QLineEdit,
    QMainWindow,
    QSizePolicy,
    QVBoxLayout,
    QStatusBar,
)

from dirtools.fileview.detail_view import DetailView
from dirtools.fileview.thumb_view import ThumbView


class FileFilter(QLineEdit):

    def __init__(self, *args):
        super().__init__(*args)


class FilePathLineEdit(QLineEdit):

    def __init__(self, controller):
        super().__init__()
        self.controller = controller
        self.returnPressed.connect(self.onReturnPressed)

    def onReturnPressed(self):
        if os.path.exists(self.text()):
            self.controller.set_directory(self.text())


class FileViewWindow(QMainWindow):

    def __init__(self, *args):
        super().__init__(*args)

        self.setWindowTitle("dt-fileview")
        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = DetailView(self)
        self.file_view.hide()
        self.thumb_view = ThumbView(self)
        self.file_path = FilePathLineEdit(self)
        self.file_filter = FileFilter()
        self.file_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.thumb_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.status_bar = QStatusBar(self)

        self.vbox.addWidget(self.file_path)
        self.vbox.addWidget(self.file_view, Qt.AlignLeft)
        self.vbox.addWidget(self.thumb_view, Qt.AlignLeft)
        self.vbox.addWidget(self.file_filter)
        self.vbox.addWidget(self.status_bar)

        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

        exitAct = QAction(QIcon.fromTheme('exit'), '&Exit', self)
        exitAct.setShortcut('Ctrl+Q')
        exitAct.setStatusTip('Exit application')
        exitAct.triggered.connect(self.close)

        back_action = QAction(QIcon.fromTheme('back'), 'Go &back', self)
        back_action.setShortcut('Alt+Left')
        back_action.setStatusTip('Go back in history')
        back_action.setEnabled(False)

        forward_action = QAction(QIcon.fromTheme('forward'), 'Go &forward', self)
        forward_action.setShortcut('Alt+Right')
        forward_action.setStatusTip('Go forward in history')
        forward_action.setEnabled(False)

        view_detail_view = QAction("Detail View", self, checkable=True)
        view_icon_view = QAction("Icon View", self, checkable=True)
        view_small_icon_view = QAction("Small Icon View", self, checkable=True)

        view_action_group = QActionGroup(self)
        view_action_group.addAction(view_detail_view)
        view_action_group.addAction(view_icon_view)
        view_action_group.addAction(view_small_icon_view)

        show_abspath_action = QAction("Show AbsPath", self, checkable=True)
        show_abspath_action.triggered.connect(self.file_view.show_abspath)

        show_basename_action = QAction("Show Basename", self, checkable=True)
        show_basename_action.triggered.connect(self.file_view.show_basename)

        path_options_group = QActionGroup(self)
        path_options_group.addAction(show_abspath_action)
        path_options_group.addAction(show_basename_action)

        about_action = QAction(QIcon.fromTheme('about'), 'About dt-fileview', self)
        about_action.setStatusTip('Show About dialog')

        menubar = self.menuBar()
        file_menu = menubar.addMenu('&File')
        file_menu.addAction(exitAct)

        view_menu = menubar.addMenu('&View')
        view_menu.addSeparator().setText("View Style")
        view_menu.addAction(view_detail_view)
        view_menu.addAction(view_icon_view)
        view_menu.addAction(view_small_icon_view)
        view_menu.addSeparator().setText("Path Options")
        view_menu.addAction("Show AbsPath", self.file_view.show_abspath)
        view_menu.addAction("Show Basename", self.file_view.show_basename)
        view_menu.addSeparator()

        history_menu = menubar.addMenu('&History')
        history_menu.addAction(back_action)
        history_menu.addAction(forward_action)

        help_menu = menubar.addMenu('&Help')
        help_menu.addAction(about_action)

        self.toolbar = self.addToolBar("FileView")
        self.toolbar.addAction(back_action)
        self.toolbar.addAction(forward_action)
        self.toolbar.addSeparator()
        self.toolbar.addAction("Show AbsPath", self.file_view.show_abspath)
        self.toolbar.addAction("Show Basename", self.file_view.show_basename)
        self.toolbar.addSeparator()
        self.toolbar.addAction("Show Time Gaps", self.file_view.toggle_timegaps)
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
        self.toolbar.addAction(QIcon.fromTheme('zoom-in'), "Zoom In", self.zoom_in)
        self.toolbar.addAction(QIcon.fromTheme('zoom-out'), "Zoom Out", self.zoom_out)
        self.toolbar.addSeparator()
        info = QLabel("lots of files selected")
        self.toolbar.addWidget(info)

        self.file_filter.setFocus()

    def show_detail_view(self):
        self.thumb_view.hide()
        self.file_view.show()

    def zoom_in(self):
        self.thumb_view.zoom_in()

    def zoom_out(self):
        self.thumb_view.zoom_out()

    def show_thumb_view(self):
        self.thumb_view.show()
        self.file_view.hide()

    def set_directory(self, path):
        self.path = path
        self.file_path.setText(self.path)

    def set_filename(self, filename):
        self.status_bar.showMessage(filename)


# EOF #
