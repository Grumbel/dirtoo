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


import logging

from PyQt5.QtCore import QObject
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (
    QAction,
    QActionGroup,
    QStyle
)

from dirtools.fileview.about_dialog import AboutDialog
from dirtools.fileview.file_info import FileInfo

logger = logging.getLogger(__name__)


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

        def on_debug(enabled):
            if enabled:
                logging.getLogger().setLevel(logging.DEBUG)
            else:
                logging.getLogger().setLevel(logging.ERROR)

        self.debug = QAction(QIcon.fromTheme('media-record'), '&Debug', self, checkable=True)
        self.debug.setShortcut('Ctrl+Q')
        self.debug.setStatusTip('Debug application')
        self.debug.setChecked(logging.getLogger().getEffectiveLevel() == logging.DEBUG)
        self.debug.triggered.connect(lambda: on_debug(self.debug.isChecked()))

        self.exit = QAction(QIcon.fromTheme('exit'), '&Exit', self)
        self.exit.setShortcut('Ctrl+Q')
        self.exit.setStatusTip('Exit application')
        self.exit.triggered.connect(lambda: self.controller.window.close())

        self.home = QAction(QIcon.fromTheme('go-home'), '&Go to Home', self)
        self.home.setStatusTip('Go to the Home directory')
        self.home.triggered.connect(self.controller.go_home)

        self.undo = QAction(QIcon.fromTheme('undo'), '&Undo', self)
        self.undo.setShortcut('Ctrl+Z')
        self.undo.setStatusTip('Undo the last action')

        self.redo = QAction(QIcon.fromTheme('redo'), '&Redo', self)
        self.redo.setShortcut('Ctrl+Y')
        self.redo.setStatusTip('Redo the last action')

        self.edit_copy = QAction(QIcon.fromTheme('edit-copy'), '&Copy', self)
        self.edit_copy.setShortcut('Ctrl+C')
        self.edit_copy.setStatusTip('Copy Selected Files')

        self.edit_cut = QAction(QIcon.fromTheme('edit-cut'), 'Cu&t', self)
        self.edit_cut.setShortcut('Ctrl+X')
        self.edit_cut.setStatusTip('Cut Selected Files')

        self.edit_paste = QAction(QIcon.fromTheme('edit-paste'), '&Paste', self)
        self.edit_paste.setShortcut('Ctrl+V')
        self.edit_paste.setStatusTip('Paste Files')

        self.edit_delete = QAction(QIcon.fromTheme('edit-delete'), '&Delete', self)
        self.edit_delete.setStatusTip('Delete Selected Files')

        self.edit_select_all = QAction(QIcon.fromTheme('edit-select-all'), '&Select All', self)
        self.edit_select_all.setShortcut('Ctrl+A')
        self.edit_select_all.setStatusTip('Select All')
        self.edit_select_all.triggered.connect(self.controller.select_all)

        self.zoom_in = QAction(QIcon.fromTheme('zoom-in'), "Zoom &In", self)
        self.zoom_in.triggered.connect(self.controller.zoom_in)
        self.zoom_in.setShortcut('Ctrl+=')
        self.zoom_out = QAction(QIcon.fromTheme('zoom-out'), "Zoom &Out", self)
        self.zoom_out.triggered.connect(self.controller.zoom_out)
        self.zoom_out.setShortcut('Ctrl+-')

        self.lod_in = QAction(QIcon.fromTheme('zoom-in'), "Level of Detail &In", self)
        self.lod_in.triggered.connect(self.controller.more_details)
        self.lod_in.setShortcut('Alt+=')
        self.lod_out = QAction(QIcon.fromTheme('zoom-out'), "Level of Detail &Out", self)
        self.lod_out.triggered.connect(self.controller.less_details)
        self.lod_out.setShortcut('Alt+-')

        self.crop_thumbnails = QAction(QIcon.fromTheme('zoom-fit-best'), "Crop Thumbnails", self, checkable=True)
        self.crop_thumbnails.triggered.connect(
            lambda: self.controller.set_crop_thumbnails(self.crop_thumbnails.isChecked()))

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

        self.reload_thumbnails = QAction(QIcon.fromTheme('edit-delete'), 'Thumbnail Reload', self)
        self.reload_thumbnails.setShortcut('Shift+F5')
        self.reload_thumbnails.setStatusTip('Reload Thumbnails')
        self.reload_thumbnails.triggered.connect(self.controller.reload_thumbnails)

        self.prepare = QAction(QIcon.fromTheme('media-playback-start'), 'Load Thumbnails', self)
        self.prepare.setShortcut('F6')
        self.prepare.setStatusTip('Load Thumbnails')
        self.prepare.triggered.connect(self.controller.prepare)

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

        self.show_hidden = QAction(QIcon.fromTheme('camera-photo'), "Show Hidden", self, checkable=True)
        self.show_hidden.triggered.connect(self.controller.show_hidden)
        self.show_hidden.setShortcut('Ctrl+H')

        self.show_filtered = QAction(QIcon.fromTheme('camera-photo'), "Show Filtered", self, checkable=True)
        self.show_filtered.triggered.connect(self.controller.show_filtered)

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
        self.sort_by_name.triggered.connect(lambda: self.controller.sorter.set_key_func(FileInfo.basename))

        self.sort_by_size = QAction("Sort by Size", checkable=True)
        self.sort_by_size.triggered.connect(lambda: self.controller.sorter.set_key_func(FileInfo.size))

        self.sort_by_ext = QAction("Sort by Extension", checkable=True)
        self.sort_by_ext.triggered.connect(lambda: self.controller.sorter.set_key_func(FileInfo.ext))

        self.sort_by_date = QAction("Sort by Date", checkable=True)
        self.sort_by_date.triggered.connect(lambda: self.controller.sorter.set_key_func(FileInfo.mtime))

        def framerate_key(fileinfo):
            metadata = fileinfo.metadata()
            return metadata.get('framerate', 0)

        self.sort_by_framerate = QAction("Sort by Framerate", checkable=True)
        self.sort_by_framerate.triggered.connect(lambda: self.controller.sorter.set_key_func(framerate_key))

        def aspect_ratio_key(fileinfo):
            metadata = fileinfo.metadata()
            if 'width' in metadata and 'height' in metadata and metadata['height'] != 0:
                return metadata['width'] / metadata['height']
            else:
                return 0

        self.sort_by_aspect_ratio = QAction("Sort by Aspect Ratio", checkable=True)
        self.sort_by_aspect_ratio.triggered.connect(lambda: self.controller.sorter.set_key_func(aspect_ratio_key))

        def area_key(fileinfo):
            metadata = fileinfo.metadata()
            if 'width' in metadata and 'height' in metadata:
                return metadata['width'] * metadata['height']
            else:
                return 0

        self.sort_by_area = QAction("Sort by Area", checkable=True)
        self.sort_by_area.triggered.connect(lambda: self.controller.sorter.set_key_func(area_key))

        def duration_key(fileinfo):
            metadata = fileinfo.metadata()
            if 'duration' in metadata:
                return metadata['duration']
            else:
                return 0

        self.sort_by_duration = QAction("Sort by Duration", checkable=True)
        self.sort_by_duration.triggered.connect(lambda: self.controller.sorter.set_key_func(duration_key))

        self.sort_by_user = QAction("Sort by User", checkable=True)
        self.sort_by_group = QAction("Sort by Group", checkable=True)
        self.sort_by_permission = QAction("Sort by Permission", checkable=True)
        self.sort_by_random = QAction("Random Shuffle", checkable=True)
        self.sort_by_random.triggered.connect(lambda: self.controller.sorter.set_key_func(None))

        self.sort_group = QActionGroup(self)
        self.sort_group.addAction(self.sort_by_name)
        self.sort_group.addAction(self.sort_by_size)
        self.sort_group.addAction(self.sort_by_ext)
        self.sort_group.addAction(self.sort_by_date)
        self.sort_group.addAction(self.sort_by_area)
        self.sort_group.addAction(self.sort_by_duration)
        self.sort_group.addAction(self.sort_by_aspect_ratio)
        self.sort_group.addAction(self.sort_by_framerate)
        self.sort_group.addAction(self.sort_by_user)
        self.sort_group.addAction(self.sort_by_group)
        self.sort_group.addAction(self.sort_by_permission)
        self.sort_group.addAction(self.sort_by_random)

        self.group_by_none = QAction("Don't Group", checkable=True)
        self.group_by_none.triggered.connect(lambda: self.controller.set_grouper_by_none())

        self.group_by_day = QAction("Group by Day", checkable=True)
        self.group_by_day.triggered.connect(lambda: self.controller.set_grouper_by_day())

        self.group_by_directory = QAction("Group by Directory", checkable=True)
        self.group_by_directory.triggered.connect(lambda: self.controller.set_grouper_by_directory())

        self.group_group = QActionGroup(self)
        self.group_group.addAction(self.group_by_none)
        self.group_group.addAction(self.group_by_day)
        self.group_group.addAction(self.group_by_directory)

        self.about = QAction(QIcon.fromTheme('help-about'), 'About dt-fileview', self)
        self.about.setStatusTip('Show About dialog')

        self.about_dialog = AboutDialog()
        self.about.triggered.connect(self.about_dialog.show)


# EOF #
