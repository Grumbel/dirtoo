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
from PyQt5.QtWidgets import (
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

        self.toolbar = self.addToolBar("FileView")
        self.toolbar.addAction("Show AbsPath", self.file_view.show_abspath)
        self.toolbar.addAction("Show Basename", self.file_view.show_basename)
        self.toolbar.addAction("Show Time Gaps", self.file_view.toggle_timegaps)
        self.toolbar.addAction("Thumbnail View", self.show_thumb_view)
        self.toolbar.addAction("Detail View", self.show_detail_view)
        info = QLabel("lots of files selected")
        self.toolbar.addWidget(info)

        self.file_filter.setFocus()

    def show_detail_view(self):
        self.thumb_view.hide()
        self.file_view.show()

    def show_thumb_view(self):
        self.thumb_view.show()
        self.file_view.hide()

    def set_directory(self, path):
        self.path = path
        self.file_path.setText(self.path)

    def set_filename(self, filename):
        self.status_bar.showMessage(filename)


# EOF #
