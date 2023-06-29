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


from typing import TYPE_CHECKING, cast

import logging

from PyQt6.QtCore import QObject
from PyQt6.QtGui import (
    QAction,
    QActionGroup,
)
from PyQt6.QtWidgets import (
    QStyle
)

from dirtoo.filesystem.file_info import FileInfo
from dirtoo.fileview.settings import settings
from dirtoo.image.icon import load_icon
from dirtoo.sort import numeric_sort_key

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller

logger = logging.getLogger(__name__)


class Actions(QObject):

    def __init__(self, controller: 'Controller') -> None:
        super().__init__()

        self.controller = controller

        self.make_actions()

    def make_actions(self) -> None:
        self.save_as = QAction(load_icon('save-as'), 'Save Filelist As', self)
        self.save_as.setShortcut('Ctrl+s')
        self.save_as.setStatusTip('Save the current file selection')
        self.save_as.triggered.connect(self.controller.save_as)

        def on_debug(enabled: bool) -> None:
            if enabled:
                logging.getLogger().setLevel(logging.DEBUG)
            else:
                logging.getLogger().setLevel(logging.ERROR)

        self.debug = QAction(load_icon('media-record'), '&Debug', self)
        self.debug.setCheckable(True)
        self.debug.setStatusTip('Debug application')
        self.debug.setChecked(logging.getLogger().getEffectiveLevel() == logging.DEBUG)
        self.debug.triggered.connect(lambda: on_debug(self.debug.isChecked()))

        self.exit = QAction(load_icon('window-close'), '&Exit', self)
        self.exit.setShortcut('Ctrl+W')
        self.exit.setStatusTip('Close Window')
        self.exit.triggered.connect(self.controller.close_window)

        self.home = QAction(load_icon('go-home'), '&Go to Home', self)
        self.home.setStatusTip('Go to the Home directory')
        self.home.triggered.connect(self.controller.go_home)

        self.undo = QAction(load_icon('undo'), '&Undo', self)
        self.undo.setShortcut('Ctrl+Z')
        self.undo.setStatusTip('Undo the last action')

        self.redo = QAction(load_icon('redo'), '&Redo', self)
        self.redo.setShortcut('Ctrl+Y')
        self.redo.setStatusTip('Redo the last action')

        self.edit_copy = QAction(load_icon('edit-copy'), '&Copy', self)
        self.edit_copy.setShortcut('Ctrl+C')
        self.edit_copy.setStatusTip('Copy Selected Files')
        self.edit_copy.triggered.connect(self.controller.on_edit_copy)

        self.edit_cut = QAction(load_icon('edit-cut'), 'Cu&t', self)
        self.edit_cut.setShortcut('Ctrl+X')
        self.edit_cut.setStatusTip('Cut Selected Files')
        self.edit_cut.triggered.connect(self.controller.on_edit_cut)

        self.edit_paste = QAction(load_icon('edit-paste'), '&Paste', self)
        self.edit_paste.setShortcut('Ctrl+V')
        self.edit_paste.setStatusTip('Paste Files')
        self.edit_paste.triggered.connect(self.controller.on_edit_paste)

        self.edit_delete = QAction(load_icon('edit-delete'), '&Delete', self)
        self.edit_delete.setStatusTip('Delete Selected Files')

        self.edit_select_all = QAction(load_icon('edit-select-all'), '&Select All', self)
        self.edit_select_all.setShortcut('Ctrl+A')
        self.edit_select_all.setStatusTip('Select All')
        self.edit_select_all.triggered.connect(self.controller.select_all)

        self.zoom_in = QAction(load_icon('zoom-in'), "Zoom &In", self)
        self.zoom_in.triggered.connect(self.controller.zoom_in)
        self.zoom_in.setShortcut('Ctrl+=')
        self.zoom_out = QAction(load_icon('zoom-out'), "Zoom &Out", self)
        self.zoom_out.triggered.connect(self.controller.zoom_out)
        self.zoom_out.setShortcut('Ctrl+-')

        self.lod_in = QAction(load_icon('zoom-in'), "Level of Detail &In", self)
        self.lod_in.triggered.connect(self.controller.more_details)
        self.lod_in.setShortcut('Alt+=')
        self.lod_out = QAction(load_icon('zoom-out'), "Level of Detail &Out", self)
        self.lod_out.triggered.connect(self.controller.less_details)
        self.lod_out.setShortcut('Alt+-')

        self.crop_thumbnails = QAction(load_icon('zoom-fit-best'), "Crop Thumbnails", self)
        self.crop_thumbnails.setCheckable(True)
        self.crop_thumbnails.triggered.connect(
            lambda: self.controller.set_crop_thumbnails(self.crop_thumbnails.isChecked()))

        self.new_window = QAction(load_icon('window-new'), "New Window", self)
        self.new_window.triggered.connect(lambda x: self.controller.new_controller(clone=True))  # type: ignore
        self.new_window.setShortcut('Ctrl+N')

        self.parent_directory = \
            QAction(self.controller.app.qapp.style().standardIcon(QStyle.StandardPixmap.SP_FileDialogToParent),
                    "Parent Directory")
        self.parent_directory.triggered.connect(self.controller.parent_directory)

        self.back = QAction(load_icon('back'), 'Go &back', self)
        self.back.setShortcut('Alt+Left')
        self.back.setStatusTip('Go back in history')
        self.back.setEnabled(False)
        self.back.triggered.connect(self.controller.go_back)

        self.forward = QAction(load_icon('forward'), 'Go &forward', self)
        self.forward.setShortcut('Alt+Right')
        self.forward.setStatusTip('Go forward in history')
        self.forward.setEnabled(False)
        self.forward.triggered.connect(self.controller.go_forward)

        self.search = QAction(load_icon('system-search'), 'Search', self)
        self.search.setShortcut('F3')
        self.search.setStatusTip('Search for files')
        self.search.triggered.connect(self.controller.show_search)

        self.reload = QAction(load_icon('reload'), 'Reload', self)
        self.reload.setShortcut('F5')
        self.reload.setStatusTip('Reload the View')
        self.reload.triggered.connect(self.controller.reload)

        self.rename = QAction(load_icon('rename'), 'Rename', self)
        self.rename.setShortcut('F2')
        self.rename.setStatusTip('Rename the current file')
        self.rename.triggered.connect(lambda checked: self.controller.show_rename_dialog())

        self.reload_thumbnails = QAction(load_icon('edit-delete'), 'Reload Thumbnails', self)
        self.reload_thumbnails.setStatusTip('Reload Thumbnails')
        self.reload_thumbnails.triggered.connect(self.controller.reload_thumbnails)

        self.reload_metadata = QAction(load_icon('edit-delete'), 'Reload MetaData', self)
        self.reload_metadata.setStatusTip('Reload MetaData')
        self.reload_metadata.triggered.connect(self.controller.reload_metadata)

        self.make_directory_thumbnails = QAction(load_icon('folder'), 'Make Directory Thumbnails', self)
        self.make_directory_thumbnails.setStatusTip('Make Directory Thumbnails')
        self.make_directory_thumbnails.triggered.connect(self.controller.make_directory_thumbnails)

        self.prepare = QAction(load_icon('media-playback-start'), 'Load Thumbnails', self)
        self.prepare.setShortcut('F6')
        self.prepare.setStatusTip('Load Thumbnails')
        self.prepare.triggered.connect(self.controller.prepare)

        self.view_detail_view = QAction("Detail View", self)
        self.view_detail_view.setCheckable(True)
        self.view_icon_view = QAction("Icon View", self)
        self.view_icon_view.setCheckable(True)
        self.view_small_icon_view = QAction("Small Icon View", self)
        self.view_small_icon_view.setCheckable(True)

        self.view_icon_view = QAction(load_icon("view-grid-symbolic"),
                                      "Icon View")
        self.view_icon_view.triggered.connect(self.controller.view_icon_view)

        self.view_small_icon_view = QAction(load_icon("view-list-symbolic"),
                                            "Small Icon View")
        self.view_small_icon_view.triggered.connect(self.controller.view_small_icon_view)

        self.view_detail_view = QAction(load_icon("view-more-horizontal-symbolic"),
                                        "Detail View")
        self.view_detail_view.triggered.connect(self.controller.view_detail_view)

        self.view_group = QActionGroup(self)
        self.view_group.addAction(self.view_detail_view)
        self.view_group.addAction(self.view_icon_view)
        self.view_group.addAction(self.view_small_icon_view)

        self.show_hidden = QAction(load_icon('camera-photo'), "Show Hidden", self)
        self.show_hidden.setCheckable(True)
        self.show_hidden.triggered.connect(self.controller.show_hidden)
        self.show_hidden.setShortcut('Ctrl+H')
        self.show_hidden.setChecked(settings.value("globals/show_hidden", False, bool))

        self.show_filtered = QAction(load_icon('camera-photo'), "Show Filtered", self)
        self.show_filtered.setCheckable(True)
        self.show_filtered.triggered.connect(self.controller.show_filtered)

        self.show_abspath = QAction("Show AbsPath", self)
        self.show_abspath.setCheckable(True)
        self.show_abspath.triggered.connect(self.controller.show_abspath)

        self.show_basename = QAction("Show Basename", self)
        self.show_basename.setCheckable(True)
        self.show_basename.triggered.connect(self.controller.show_basename)

        self.path_options_group = QActionGroup(self)
        self.path_options_group.addAction(self.show_abspath)
        self.path_options_group.addAction(self.show_basename)

        self.toggle_timegaps = QAction("Show Time Gaps", self)
        self.toggle_timegaps.setCheckable(True)
        self.toggle_timegaps.triggered.connect(self.controller.toggle_timegaps)

        # Sorting Options
        self.sort_directories_first = QAction("Directories First")
        self.sort_directories_first.setCheckable(True)
        self.sort_directories_first.triggered.connect(
            lambda: self.controller._sorter.set_directories_first(self.sort_directories_first.isChecked()))
        self.sort_directories_first.setChecked(True)

        self.sort_reversed = QAction("Reverse Sort")
        self.sort_reversed.setCheckable(True)
        self.sort_reversed.triggered.connect(
            lambda: self.controller.set_sort_reversed(self.sort_reversed.isChecked()))

        self.sort_by_name = QAction("Sort by Name")
        self.sort_by_name.setCheckable(True)
        self.sort_by_name.triggered.connect(lambda:
                                            self.controller.set_sort_key_func(
                                                lambda x: numeric_sort_key(x.basename().lower())))
        self.sort_by_name.setChecked(True)

        self.sort_by_size = QAction("Sort by Size")
        self.sort_by_size.setCheckable(True)
        self.sort_by_size.triggered.connect(lambda: self.controller.set_sort_key_func(FileInfo.size))

        self.sort_by_ext = QAction("Sort by Extension")
        self.sort_by_ext.setCheckable(True)
        self.sort_by_ext.triggered.connect(lambda: self.controller.set_sort_key_func(FileInfo.ext))

        self.sort_by_date = QAction("Sort by Date")
        self.sort_by_date.setCheckable(True)
        self.sort_by_date.triggered.connect(lambda: self.controller.set_sort_key_func(FileInfo.mtime))

        def framerate_key(fileinfo: FileInfo) -> float:
            return cast(float, fileinfo.get_metadata_or('framerate', 0.0))

        self.sort_by_framerate = QAction("Sort by Framerate")
        self.sort_by_framerate.setCheckable(True)
        self.sort_by_framerate.triggered.connect(lambda: self.controller.set_sort_key_func(framerate_key))

        def aspect_ratio_key(fileinfo: FileInfo) -> float:
            if fileinfo.has_metadata('width') and fileinfo.has_metadata('height'):
                try:
                    width = int(fileinfo.get_metadata('width'))
                    height = int(fileinfo.get_metadata('height'))
                    if height == 0:
                        logger.error("{}: height is 0".format(fileinfo))
                        return 0
                    return width / height
                except (ValueError, TypeError):
                    logger.exception("invalid width/height metadata")
                    return 0
            else:
                return 0

        self.sort_by_aspect_ratio = QAction("Sort by Aspect Ratio")
        self.sort_by_aspect_ratio.setCheckable(True)
        self.sort_by_aspect_ratio.triggered.connect(lambda: self.controller.set_sort_key_func(aspect_ratio_key))

        def resolution_key(fileinfo: FileInfo) -> int:
            width = cast(int, fileinfo.get_metadata_or('width', 0))
            height = cast(int, fileinfo.get_metadata_or('height', 0))
            return width * height

        self.sort_by_resolution = QAction("Sort by Resolution")
        self.sort_by_resolution.setCheckable(True)
        self.sort_by_resolution.triggered.connect(lambda: self.controller.set_sort_key_func(resolution_key))

        def duration_key(fileinfo: FileInfo) -> int:
            return cast(int, fileinfo.get_metadata_or('duration', 0))

        self.sort_by_duration = QAction("Sort by Duration")
        self.sort_by_duration.setCheckable(True)
        self.sort_by_duration.triggered.connect(lambda: self.controller.set_sort_key_func(duration_key))

        self.sort_by_user = QAction("Sort by User")
        self.sort_by_user.setCheckable(True)
        self.sort_by_group = QAction("Sort by Group")
        self.sort_by_group.setCheckable(True)
        self.sort_by_permission = QAction("Sort by Permission")
        self.sort_by_permission.setCheckable(True)
        self.sort_by_random = QAction("Random Shuffle")
        self.sort_by_random.setCheckable(True)
        # self.sort_by_random.triggered.connect(lambda: self.controller.set_sort_key_func(None))
        self.sort_by_random.triggered.connect(lambda: print("sort_by_random: not implemented"))

        self.sort_group = QActionGroup(self)
        self.sort_group.addAction(self.sort_by_name)
        self.sort_group.addAction(self.sort_by_size)
        self.sort_group.addAction(self.sort_by_ext)
        self.sort_group.addAction(self.sort_by_date)
        self.sort_group.addAction(self.sort_by_resolution)
        self.sort_group.addAction(self.sort_by_duration)
        self.sort_group.addAction(self.sort_by_aspect_ratio)
        self.sort_group.addAction(self.sort_by_framerate)
        self.sort_group.addAction(self.sort_by_user)
        self.sort_group.addAction(self.sort_by_group)
        self.sort_group.addAction(self.sort_by_permission)
        self.sort_group.addAction(self.sort_by_random)

        self.group_by_none = QAction("Don't Group")
        self.group_by_none.setCheckable(True)
        self.group_by_none.triggered.connect(self.controller.set_grouper_by_none)
        self.group_by_none.setChecked(True)

        self.group_by_day = QAction("Group by Day")
        self.group_by_day.setCheckable(True)
        self.group_by_day.triggered.connect(self.controller.set_grouper_by_day)

        self.group_by_directory = QAction("Group by Directory")
        self.group_by_directory.setCheckable(True)
        self.group_by_directory.triggered.connect(self.controller.set_grouper_by_directory)

        self.group_by_duration = QAction("Group by Duration")
        self.group_by_duration.setCheckable(True)
        self.group_by_duration.triggered.connect(self.controller.set_grouper_by_duration)

        self.group_group = QActionGroup(self)
        self.group_group.addAction(self.group_by_none)
        self.group_group.addAction(self.group_by_day)
        self.group_group.addAction(self.group_by_directory)
        self.group_group.addAction(self.group_by_duration)

        self.about = QAction(load_icon('help-about'), 'About dirtoo', self)
        self.about.setStatusTip('Show About dialog')

        self.about.triggered.connect(self.controller.show_about_dialog)

        self.show_preferences = QAction(load_icon("preferences-system"), "Preferencs...")
        self.show_preferences.triggered.connect(self.controller.show_preferences)

        def on_filter_pin(checked: bool) -> None:
            # FIXME: Could use icon state for this
            if checked:
                self.filter_pin.setIcon(load_icon("remmina-pin-down"))
            else:
                self.filter_pin.setIcon(load_icon("remmina-pin-up"))

        self.filter_pin = QAction(load_icon("remmina-pin-up"), "Pin the filter")
        self.filter_pin.setCheckable(True)
        self.filter_pin.triggered.connect(self.controller.set_filter_pin)
        # self.filter_pin.setToolTip("Pin the filter")

        self.filter_pin.triggered.connect(on_filter_pin)


# EOF #
