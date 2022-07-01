# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018-2022 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import TYPE_CHECKING, Union

import logging
import traceback
import os

from PyQt5.QtCore import QObject, pyqtSignal
from inotify_simple import flags as inotify_flags
import inotify_simple

from dirtoo.file_info import FileInfo
from dirtoo.location import Location
from dirtoo.watcher.inotify_qt import INotifyQt

if TYPE_CHECKING:
    from dirtoo.fileview.virtual_filesystem import VirtualFilesystem
    from dirtoo.fileview.stdio_filesystem import StdioFilesystem


logger = logging.getLogger(__name__)


class DirectoryWatcherWorker(QObject):

    sig_file_added = pyqtSignal(FileInfo)
    sig_file_removed = pyqtSignal(Location)
    sig_file_modified = pyqtSignal(FileInfo)
    sig_file_closed = pyqtSignal(FileInfo)
    sig_error = pyqtSignal()
    sig_scandir_finished = pyqtSignal(list)
    sig_message = pyqtSignal(str)

    def __init__(self, vfs: Union['StdioFilesystem', 'VirtualFilesystem'], location: Location) -> None:
        super().__init__()

        self.vfs = vfs
        self.location = location
        self.path = self.vfs.get_stdio_name(location)
        self._close = False

    def init(self) -> None:
        try:
            self.inotify = INotifyQt(self)
            self.inotify.add_watch(self.path)
            self.inotify.sig_event.connect(self.on_inotify_event)
            self.process()
        except Exception as err:
            self.sig_message.emit(str(err))

    def close(self) -> None:
        self.inotify.close()
        del self.inotify

    def process(self) -> None:
        fileinfos = []

        logger.debug("DirectoryWatcher.process: gather directory content")
        for entry in os.listdir(self.path):
            location = Location.join(self.location, entry)
            fileinfo = self.vfs.get_fileinfo(location)
            fileinfos.append(fileinfo)

            if self._close:
                return

        self.sig_scandir_finished.emit(fileinfos)

    def on_inotify_event(self, ev: inotify_simple.Event) -> None:
        try:
            logger.debug("inotify-event: name: '%s'  mask: %s",
                         ev.name,
                         ", ".join([str(x)
                                    for x in inotify_flags.from_mask(ev.mask)]))

            location = Location.join(self.location, ev.name)

            if ev.mask & inotify_flags.CREATE:
                self.sig_file_added.emit(self.vfs.get_fileinfo(location))
            elif ev.mask & inotify_flags.DELETE:
                self.sig_file_removed.emit(location)
            elif ev.mask & inotify_flags.DELETE_SELF:
                pass  # directory itself has disappeared
            elif ev.mask & inotify_flags.MOVE_SELF:
                pass  # directory itself has moved
            elif ev.mask & inotify_flags.MODIFY or ev.mask & inotify_flags.ATTRIB:
                self.sig_file_modified.emit(self.vfs.get_fileinfo(location))
            elif ev.mask & inotify_flags.MOVED_FROM:
                self.sig_file_removed.emit(location)
            elif ev.mask & inotify_flags.MOVED_TO:
                self.sig_file_added.emit(self.vfs.get_fileinfo(location))
            elif ev.mask & inotify_flags.CLOSE_WRITE:
                self.sig_file_closed.emit(self.vfs.get_fileinfo(location))
            else:
                # unhandled event
                print("ERROR: Unhandled flags:")
                for flag in inotify_flags.from_mask(ev.mask):
                    print('    ' + str(flag))
        except Exception as err:
            print(traceback.format_exc())
            print("DirectoryWatcher:", err)


# EOF #
