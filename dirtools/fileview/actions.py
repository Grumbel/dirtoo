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
    QActionGroup,
    QStyle
)

from dirtools.fileview.about_dialog import AboutDialog
from dirtools.fileview.file_info import FileInfo


class Actions(QObject):

    def __init__(self, controller):
        super().__init__()

        self.controller = controller

        self.make_actions()

    def make_actions(self):
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

        self.parent_directory = QAction(self.controller.app.qapp.style().standardIcon(QStyle.SP_FileDialogToParent),
                                        "Parent Directory")
        self.parent_directory.triggered.connect(self.controller.parent_directory)

        self.back = QAction(QIcon.fromTheme('back'), 'Go &back', self)
        self.back.setShortcut('Alt+Left')
        self.back.setStatusTip('Go back in history')
        self.back.setEnabled(False)
        self.back.triggered.connect(self.controller.go_back)

        self.forward = QAction(QIcon.fromTheme('forward'), 'Go &forward', self)
        self.forward.setShortcut('Alt+Right')
        self.forward.setStatusTip('Go forward in history')
        self.forward.setEnabled(False)
        self.forward.triggered.connect(self.controller.go_forward)

        self.reload = QAction(QIcon.fromTheme('reload'), 'Reload', self)
        self.reload.setShortcut('F5')
        self.reload.setStatusTip('Reload the View')
        self.reload.triggered.connect(self.controller.reload)

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

        self.show_hidden = QAction("Show Hidden", self, checkable=True)
        self.show_hidden.triggered.connect(self.controller.show_hidden)

        self.show_abspath = QAction("Show AbsPath", self, checkable=True)
        self.show_abspath.triggered.connect(self.controller.show_abspath)

        self.show_basename = QAction("Show Basename", self, checkable=True)
        self.show_basename.triggered.connect(self.controller.show_basename)

        self.path_options_group = QActionGroup(self)
        self.path_options_group.addAction(self.show_abspath)
        self.path_options_group.addAction(self.show_basename)

        self.toggle_timegaps = QAction("Show Time Gaps", self, checkable=True)
        self.toggle_timegaps.triggered.connect(self.controller.toggle_timegaps)

        # Sorting Options
        self.sort_directories_first = QAction("Directories First", checkable=True)
        self.sort_directories_first.triggered.connect(
            lambda: self.controller.sorter.set_directories_first(self.sort_directories_first.isChecked()))

        self.sort_reversed = QAction("Reverse Sort", checkable=True)
        self.sort_reversed.triggered.connect(
            lambda: self.controller.sorter.set_sort_reversed(self.sort_reversed.isChecked()))

        self.sort_by_name = QAction("Sort by Name", checkable=True)
        self.sort_by_name.triggered.connect(lambda: self.controller.sorter.set_key_func(FileInfo.filename))
        self.sort_by_size = QAction("Sort by Size", checkable=True)
        self.sort_by_size.triggered.connect(lambda: self.controller.sorter.set_key_func(FileInfo.size))
        self.sort_by_user = QAction("Sort by User", checkable=True)
        self.sort_by_group = QAction("Sort by Group", checkable=True)
        self.sort_by_permission = QAction("Sort by Permission", checkable=True)
        self.sort_by_random = QAction("Random Shuffle", checkable=True)
        self.sort_by_random.triggered.connect(lambda: self.controller.sorter.set_key_func(None))

        self.sort_group = QActionGroup(self)
        self.sort_group.addAction(self.sort_by_name)
        self.sort_group.addAction(self.sort_by_size)
        self.sort_group.addAction(self.sort_by_user)
        self.sort_group.addAction(self.sort_by_group)
        self.sort_group.addAction(self.sort_by_permission)
        self.sort_group.addAction(self.sort_by_random)

        self.about = QAction(QIcon.fromTheme('help-about'), 'About dt-fileview', self)
        self.about.setStatusTip('Show About dialog')

        self.about_dialog = AboutDialog()
        self.about.triggered.connect(self.about_dialog.show)


# EOF #
