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


from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QWidget,
    QLabel,
    QLineEdit,
    QMainWindow,
    QSizePolicy,
    QVBoxLayout,
)

from dirtools.fileview.detail_view import DetailView
from dirtools.fileview.thumb_view import ThumbView


class FileFilter(QLineEdit):

    def __init__(self, *args):
        super().__init__(*args)


class FileViewWindow(QMainWindow):

    def __init__(self, *args):
        super().__init__(*args)

        self.setWindowTitle("dt-fileview")
        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = DetailView()
        self.thumb_view = ThumbView()
        self.file_filter = FileFilter()
        self.file_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.vbox.addWidget(self.file_view, Qt.AlignLeft)
        self.thumb_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.vbox.addWidget(self.thumb_view, Qt.AlignLeft)
        self.vbox.addWidget(self.file_filter)
        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

        self.toolbar = self.addToolBar("FileView")
        self.toolbar.addAction("Show AbsPath", self.file_view.show_abspath)
        self.toolbar.addAction("Show Basename", self.file_view.show_basename)
        self.toolbar.addAction("Show Time Gaps", self.file_view.toggle_timegaps)
        self.toolbar.addAction("Thumbnail View", self.toggle_thing)
        self.toolbar.addAction("Detail View", self.toggle_thing)
        info = QLabel("lots of files selected")
        self.toolbar.addWidget(info)

        self.file_filter.setFocus()

    def toggle_thing(self):
        print("toggle_thing")


# EOF #
