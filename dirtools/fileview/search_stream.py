# dirtool.py - diff tool for directories
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


from typing import Optional

import logging
import os

from PyQt5.QtCore import QObject, pyqtSignal, QThread, Qt

from dirtools.fileview.file_info import FileInfo
from dirtools.find.action import Action
from dirtools.find.filter import SimpleFilter
from dirtools.find.walk import walk

logger = logging.getLogger(__name__)


class SearchStreamWorker(QObject):

    sig_file_added = pyqtSignal(FileInfo)
    sig_end_of_stream = pyqtSignal()
    sig_error = pyqtSignal()

    def __init__(self, abspath: str, pattern: str) -> None:
        super().__init__()
        self._abspath = abspath
        self._pattern = pattern
        self._close = False

        self._action: Optional[SearchStreamAction] = None
        self._filter: Optional[SimpleFilter] = None

    def close(self):
        pass

    def init(self) -> None:
        self._action = SearchStreamAction(self)
        self._filter = SimpleFilter.from_string(self._pattern)

        self._find_files(self._abspath, True,
                         filter_op=self._filter,
                         action=self._action,
                         topdown=False, maxdepth=None)

        self.sig_end_of_stream.emit()

    def _find_files(self, directory, recursive, filter_op, action, topdown, maxdepth):
        for root, dirs, files in walk(directory, topdown=topdown, maxdepth=maxdepth):
            for f in files:
                if filter_op.match_file(root, f):
                    action.file(root, f)

                if self._close:
                    return

            if not recursive:
                del dirs[:]


class SearchStreamAction(Action):

    def __init__(self, worker: SearchStreamWorker) -> None:
        super().__init__()

        self._worker = worker

    def file(self, root: str, filename: str) -> None:
        fullpath = os.path.join(root, filename)
        fileinfo = FileInfo.from_filename(fullpath)
        self._worker.sig_file_added.emit(fileinfo)

    def directory(self, root: str, filename: str) -> None:
        fullpath = os.path.join(root, filename)
        fileinfo = FileInfo.from_filename(fullpath)
        self._worker.sig_file_added.emit(fileinfo)

    def finish(self) -> None:
        pass


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
        self.sig_close_requested.connect(self._worker.close, type=Qt.BlockingQueuedConnection)

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        assert self._worker._close is False
        self._worker._close = True
        self.sig_close_requested.emit()
        self._thread.quit()
        self._thread.wait()

    @property
    def sig_file_added(self):
        return self._worker.sig_file_added

    @property
    def sig_finished(self):
        return self._worker.sig_end_of_stream

    @property
    def sig_error(self):
        return self._worker.sig_error


# EOF #
