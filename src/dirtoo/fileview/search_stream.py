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


from typing import cast, Any, Optional

import logging
import os

from PyQt6.QtCore import QObject, pyqtSignal, QThread, Qt

from dirtoo.filesystem.file_info import FileInfo
from dirtoo.find.action import Action
from dirtoo.find.filter import Filter, SimpleFilter
from dirtoo.find.walk import walk

logger = logging.getLogger(__name__)


class SearchStreamWorker(QObject):

    sig_file_added = pyqtSignal(FileInfo)
    sig_finished = pyqtSignal()
    sig_error = pyqtSignal()
    sig_message = pyqtSignal(str)

    def __init__(self, abspath: str, pattern: str) -> None:
        super().__init__()
        self._abspath = abspath
        self._pattern = pattern
        self._close = False

        self._action: Optional[SearchStreamAction] = None
        self._filter: Optional[SimpleFilter] = None

    def close(self) -> None:
        pass

    def init(self) -> None:
        self._action = SearchStreamAction(self)
        self._filter = SimpleFilter.from_string(self._pattern)

        self._find_files(self._abspath, True,
                         filter_op=self._filter,
                         action=self._action,
                         topdown=False, maxdepth=None)

        if self._action.found_count() == 0:
            self.sig_message.emit("Search did not give any results")

        self.sig_finished.emit()

    def _find_files(self, directory: str, recursive: bool, filter_op: Filter, action: Action,
                    topdown: bool, maxdepth: Optional[int]) -> None:
        for root, dirs, files in walk(directory, topdown=topdown, maxdepth=maxdepth):
            for f in files:
                if filter_op.match_file(cast(str, root), cast(str, f)):
                    action.file(cast(str, root), cast(str, f))

                if self._close:
                    return

            if not recursive:
                del dirs[:]


class SearchStreamAction(Action):

    def __init__(self, worker: SearchStreamWorker) -> None:
        super().__init__()

        self._found_count = 0
        self._worker = worker

    def file(self, root: str, filename: str) -> None:
        self._found_count += 1
        fullpath = os.path.join(root, filename)
        fileinfo = FileInfo.from_path(fullpath)
        self._worker.sig_file_added.emit(fileinfo)

    def directory(self, root: str, filename: str) -> None:
        self._found_count += 1
        fullpath = os.path.join(root, filename)
        fileinfo = FileInfo.from_path(fullpath)
        self._worker.sig_file_added.emit(fileinfo)

    def finish(self) -> None:
        pass

    def found_count(self) -> int:
        return self._found_count


class SearchStream(QObject):

    sig_close_requested = pyqtSignal()

    def __init__(self, abspath: str, pattern: str) -> None:
        super().__init__()
        self._worker = SearchStreamWorker(abspath, pattern)
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
    def sig_file_added(self) -> Any:
        return self._worker.sig_file_added

    @property
    def sig_finished(self) -> Any:
        return self._worker.sig_finished

    @property
    def sig_message(self) -> Any:
        return self._worker.sig_message

    @property
    def sig_error(self) -> Any:
        return self._worker.sig_error


# EOF #
