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


from PyQt5.QtCore import QObject
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (
    QAction,
    QActionGroup
)

from dirtools.fileview.about_dialog import AboutDialog


class Actions(QObject):

    def __init__(self, controller):
        super().__init__()

        self.controller = controller

        self.save_as = QAction(QIcon.fromTheme('save-as'), 'Save As', self)
        self.save_as.setShortcut('Ctrl+s')
        self.save_as.setStatusTip('Save the current file selection')
        self.save_as.triggered.connect(self.controller.save_as)

        self.exit = QAction(QIcon.fromTheme('exit'), '&Exit', self)
        self.exit.setShortcut('Ctrl+Q')
        self.exit.setStatusTip('Exit application')
        self.exit.triggered.connect(self.controller.close)

        self.undo = QAction(QIcon.fromTheme('undo'), '&Undo', self)
        self.undo.setShortcut('Ctrl+Z')
        self.undo.setStatusTip('Undo the last action')

        self.redo = QAction(QIcon.fromTheme('redo'), '&Redo', self)
        self.redo.setShortcut('Ctrl+Y')
        self.redo.setStatusTip('Redo the last action')

        self.zoom_in = QAction(QIcon.fromTheme('zoom-in'), "Zoom &In", self)
        self.zoom_in.triggered.connect(self.controller.zoom_in)
        self.zoom_out = QAction(QIcon.fromTheme('zoom-out'), "Zoom &Out", self)
        self.zoom_out.triggered.connect(self.controller.zoom_out)

        self.back = QAction(QIcon.fromTheme('back'), 'Go &back', self)
        self.back.setShortcut('Alt+Left')
        self.back.setStatusTip('Go back in history')
        self.back.setEnabled(False)

        self.forward = QAction(QIcon.fromTheme('forward'), 'Go &forward', self)
        self.forward.setShortcut('Alt+Right')
        self.forward.setStatusTip('Go forward in history')
        self.forward.setEnabled(False)

        self.view_detail_view = QAction("Detail View", self, checkable=True)
        self.view_icon_view = QAction("Icon View", self, checkable=True)
        self.view_small_icon_view = QAction("Small Icon View", self, checkable=True)

        self.view_detail_view = QAction("Detail View")
        self.view_detail_view.triggered.connect(self.controller.view_detail_view)
        self.view_icon_view = QAction("Icon View")
        self.view_icon_view.triggered.connect(self.controller.view_icon_view)
        self.view_small_icon_view = QAction("Small Icon View")
        self.view_small_icon_view.triggered.connect(self.controller.view_small_icon_view)

        self.view_group = QActionGroup(self)
        self.view_group.addAction(self.view_detail_view)
        self.view_group.addAction(self.view_icon_view)
        self.view_group.addAction(self.view_small_icon_view)

        self.show_abspath = QAction("Show AbsPath", self, checkable=True)
        self.show_abspath.triggered.connect(self.controller.show_abspath)

        self.show_basename = QAction("Show Basename", self, checkable=True)
        self.show_basename.triggered.connect(self.controller.show_basename)

        self.path_options_group = QActionGroup(self)
        self.path_options_group.addAction(self.show_abspath)
        self.path_options_group.addAction(self.show_basename)

        self.toggle_timegaps = QAction("Show Time Gaps", self, checkable=True)
        self.toggle_timegaps.triggered.connect(self.controller.toggle_timegaps)

        self.about = QAction(QIcon.fromTheme('help-about'), 'About dt-fileview', self)
        self.about.setStatusTip('Show About dialog')
        self.about.triggered.connect(lambda: AboutDialog().exec_())


# EOF #
