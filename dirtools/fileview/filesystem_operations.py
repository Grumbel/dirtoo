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


from typing import Optional, List, Callable

import logging

from PyQt5.QtCore import QObject, QThread
from PyQt5.QtWidgets import QWidget

from dirtools.fileview.location import Location
from dirtools.fileview.rename_operation import RenameOperation
from dirtools.file_transfer import FileTransfer, ConsoleMediator, ConsoleProgress

if False:
    from dirtools.filesystem import Filesystem  # noqa: F401

logger = logging.getLogger(__name__)


class TransferWorker(QObject):

    def __init__(self, fs: 'Filesystem', action: Callable, sources: List[str], destination: str) -> None:
        super().__init__()

        self._fs = fs
        self._action = action
        self._sources = sources
        self._destination = destination

    def on_started(self) -> None:
        mediator = ConsoleMediator()
        progress = ConsoleProgress()
        transfer = FileTransfer(self._fs, mediator, progress)

        for source in self._sources:
            self._action(transfer, source, self._destination)


class FilesystemOperations:

    """Filesystem operations for the GUI. Messageboxes and dialogs will be
    shown on errors and conflicts."""

    def __init__(self, app) -> None:
        self._app = app
        self._rename_op = RenameOperation()
        self._threads: List[QThread] = []
        self._workers: List[TransferWorker] = []

    def close(self) -> None:
        pass

    def rename_location(self, location: Location, parent: Optional[QWidget] = None) -> None:
        self._rename_op.rename_location(location, parent)

    def move_files(self, sources: List[str], destination: str) -> None:
        self._transfer_files(FileTransfer.move, sources, destination)
        # transfer_dialog = TransferDialog(destination)

    def copy_files(self, sources: List[str], destination: str) -> None:
        self._transfer_files(FileTransfer.copy, sources, destination)

    def link_files(self, sources: List[str], destination: str) -> None:
        self._transfer_files(FileTransfer.link, sources, destination)

    def _transfer_files(self, action: Callable, sources: List[str], destination: str) -> None:
        thread = QThread()
        worker = TransferWorker(self._app.fs, action, sources, destination)
        worker.moveToThread(thread)
        thread.started.connect(worker.on_started)
        thread.start()

        self._threads.append(thread)
        self._workers.append(worker)

    def create_file(self, path: str) -> None:
        pass

    def create_directory(self, path: str) -> None:
        pass


# EOF #
