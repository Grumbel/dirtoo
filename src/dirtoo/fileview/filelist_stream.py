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


from typing import cast, TYPE_CHECKING, Optional, IO, Generator

import logging
import fcntl
import os

from PyQt6.sip import voidptr
from PyQt6.QtCore import QObject, QSocketNotifier, pyqtSignal, pyqtBoundSignal

from dirtoo.filesystem.file_info import FileInfo
from dirtoo.filesystem.location import Location

if TYPE_CHECKING:
    from dirtoo.fileview.application import FileViewApplication
    from dirtoo.fileview.virtual_filesystem import VirtualFilesystem

logger = logging.getLogger(__name__)


def non_blocking_readline(fp: IO[str], linesep: str) -> Generator[Optional[str], None, None]:
    flag = fcntl.fcntl(fp, fcntl.F_GETFL)
    fcntl.fcntl(fp.fileno(), fcntl.F_SETFL, flag | os.O_NONBLOCK)

    rest = ""
    while True:
        try:
            # The buffer size is chosen to be artificially tiny to not
            # make the GUI unresponsive.
            data = fp.read(16)
        except BlockingIOError:
            yield None
        else:
            if data == "":
                return
            else:
                data = rest + data
                idx = data.find(linesep)
                if idx != -1:
                    rest = data[idx + 1:]
                    yield data[0:idx]
                else:
                    rest = data
                    yield None


class FileListStream(QObject):
    """FileListStream represents a stream of filenames read from stdin or
    from other sources that is visualized in the FileView.
    """

    sig_file_added = pyqtSignal(FileInfo)
    sig_end_of_stream = pyqtSignal()
    sig_error = pyqtSignal()

    @staticmethod
    def from_location(app: 'FileViewApplication', linesep: str, location: Location) -> 'FileListStream':
        if location.get_path() in ["/stdin", "stdin"]:
            tee_fd: IO[str]
            stream_id: str
            result = app.stream_manager.get_stdin()
            assert result is not None
            tee_fd, stream_id = result
        else:
            raise RuntimeError(f"FileListStream: unknown location: {location}")

        return FileListStream(app.vfs, tee_fd, linesep)

    @property
    def sig_finished(self) -> pyqtBoundSignal:
        return self.sig_end_of_stream

    def __init__(self, vfs: 'VirtualFilesystem',
                 fp: IO[str], linesep: str = "\n") -> None:
        super().__init__()

        self.vfs = vfs
        self.fp = fp
        self.linesep = linesep

        self.readliner: Optional[Generator[Optional[str], None, None]] = None
        self.socket_notifier: Optional[QSocketNotifier] = None

    def close(self) -> None:
        self.fp.close()

    def start(self) -> None:
        self.readliner = non_blocking_readline(self.fp, self.linesep)

        self.socket_notifier = QSocketNotifier(cast(voidptr, self.fp.fileno()), QSocketNotifier.Type.Read)
        self.socket_notifier.activated.connect(self._on_activated)

    def _on_activated(self, fd: int) -> None:
        assert self.readliner is not None
        assert self.socket_notifier is not None

        while True:
            try:
                filename: Optional[str] = next(self.readliner)
            except StopIteration:
                self.socket_notifier.setEnabled(False)
                self.socket_notifier = None
                self.sig_end_of_stream.emit()
                return
            else:
                if filename is not None:
                    location = Location.from_path(filename)
                    self.sig_file_added.emit(self.vfs.get_fileinfo(location))
                else:
                    return


# EOF #
