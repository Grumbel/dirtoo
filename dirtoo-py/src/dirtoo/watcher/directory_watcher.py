# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
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

from PyQt6.QtCore import Qt, QObject, QThread, pyqtSignal, pyqtBoundSignal

from dirtoo.filesystem.location import Location
from dirtoo.watcher.directory_watcher_worker import DirectoryWatcherWorker


if TYPE_CHECKING:
    from dirtoo.fileview.virtual_filesystem import VirtualFilesystem
    from dirtoo.filesystem.stdio_filesystem import StdioFilesystem


class DirectoryWatcher(QObject):

    sig_close_requested = pyqtSignal()

    def __init__(self, vfs: Union['StdioFilesystem', 'VirtualFilesystem'], location: Location) -> None:
        super().__init__()
        self._worker = DirectoryWatcherWorker(vfs, location)
        self._thread = QThread(self)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.init)

        # close() is a blocking connection so the thread is properly
        # done after the signal was emit'ed and we don't have to fuss
        # around with sig_finished() and other stuff
        self.sig_close_requested.connect(self._worker.close,
                                         type=Qt.ConnectionType.BlockingQueuedConnection)  # type: ignore

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        assert self._worker._close is False
        self._worker._close = True
        self.sig_close_requested.emit()
        self._thread.quit()
        self._thread.wait()

    @property
    def sig_file_added(self) -> pyqtBoundSignal:
        return self._worker.sig_file_added

    @property
    def sig_file_removed(self) -> pyqtBoundSignal:
        return self._worker.sig_file_removed

    @property
    def sig_file_modified(self) -> pyqtBoundSignal:
        return self._worker.sig_file_modified

    @property
    def sig_file_closed(self) -> pyqtBoundSignal:
        return self._worker.sig_file_closed

    @property
    def sig_scandir_finished(self) -> pyqtBoundSignal:
        return self._worker.sig_scandir_finished

    @property
    def sig_message(self) -> pyqtBoundSignal:
        return self._worker.sig_message


# EOF #
