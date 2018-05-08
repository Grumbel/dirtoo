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


from typing import Optional, List, Callable, TYPE_CHECKING

import logging

from PyQt5.QtCore import Qt, QObject, QThread, pyqtSignal
from PyQt5.QtWidgets import QWidget

from dirtools.fileview.location import Location
from dirtools.fileview.rename_operation import RenameOperation
from dirtools.fileview.return_value import ReturnValue
from dirtools.file_transfer import FileTransfer, Progress, Resolution, Mediator, CancellationException
from dirtools.fileview.conflict_dialog import ConflictDialog
from dirtools.fileview.transfer_dialog import TransferDialog

if TYPE_CHECKING:
    from dirtools.filesystem import Filesystem  # noqa: F401

logger = logging.getLogger(__name__)


class TransferWorker(QObject):

    def __init__(self, fs: 'Filesystem', action: Callable, sources: List[str], destination: str,
                 mediator: Mediator, progress: Progress) -> None:
        super().__init__()

        self._fs = fs
        self._action = action
        self._sources = sources
        self._destination = destination
        self._mediator = mediator
        self._progress = progress

        self._close = False

    def close(self) -> None:
        pass

    def on_started(self) -> None:
        transfer = FileTransfer(self._fs, self._mediator, self._progress)

        try:
            for source in self._sources:
                self._action(transfer, source, self._destination)
        except CancellationException:
            self._progress.transfer_canceled()
        finally:
            self._progress.transfer_completed()


class GuiProgress(QObject):

    sig_copy_directory = pyqtSignal(str, str)
    sig_copy_file = pyqtSignal(str, str)
    sig_copy_progress = pyqtSignal(int, int)
    sig_link_file = pyqtSignal(str, str)
    sig_move_file = pyqtSignal(str, str)
    sig_move_directory = pyqtSignal(str, str)
    sig_remove_file = pyqtSignal(str)
    sig_skip_copy = pyqtSignal(str, str)
    sig_skip_link = pyqtSignal(str, str)
    sig_skip_move_file = pyqtSignal(str, str)
    sig_skip_move_directory = pyqtSignal(str, str)
    sig_skip_rename = pyqtSignal(str, str)
    sig_transfer_canceled = pyqtSignal()
    sig_transfer_completed = pyqtSignal()

    def __init__(self) -> None:
        super().__init__()

    def skip_rename(self, src: str, dst: str) -> None:
        self.sig_skip_rename.emit(src, dst)

    def skip_link(self, src: str, dst: str) -> None:
        self.sig_skip_link.emit(src, dst)

    def skip_copy(self, src: str, dst: str) -> None:
        self.sig_skip_copy.emit(src, dst)

    def skip_move_directory(self, src: str, dst: str) -> None:
        self.sig_skip_move_directory.emit(src, dst)

    def copy_file(self, src: str, dst: str) -> None:
        self.sig_copy_file.emit(src, dst)

    def copy_directory(self, src: str, dst: str) -> None:
        self.sig_copy_directory.emit(src, dst)

    def remove_file(self, src: str) -> None:
        self.sig_remove_file.emit(src)

    def link_file(self, src: str, dst: str) -> None:
        self.sig_link_file.emit(src, dst)

    def move_file(self, src: str, dst: str) -> None:
        self.sig_move_file.emit(src, dst)

    def move_directory(self, src: str, dst: str) -> None:
        self.sig_move_directory.emit(src, dst)

    def copy_progress(self, current: int, total: int) -> None:
        self.sig_copy_progress.emit(current, total)

    def transfer_canceled(self) -> None:
        self.sig_transfer_canceled.emit()

    def transfer_completed(self) -> None:
        self.sig_transfer_completed.emit()


class GuiMediator(QObject):
    """Whenever a filesystem operation would result in the destruction of data,
    the Mediator is called to decide which action should be taken."""

    sig_file_conflict = pyqtSignal(ReturnValue)
    sig_directory_conflict = pyqtSignal(ReturnValue)

    def __init__(self, parent) -> None:
        super().__init__(parent)

    def cancel_transfer(self) -> bool:
        return False

    def file_conflict(self, source: str, dest: str) -> Resolution:
        retval = ReturnValue[Resolution]()
        self.sig_file_conflict.emit(retval)
        result: Resolution = retval.receive()
        return result

    def directory_conflict(self, sourcedir: str, destdir: str) -> Resolution:
        retval = ReturnValue[Resolution]()
        self.sig_file_conflict.emit(retval)
        result: Resolution = retval.receive()
        return result


class GuiFileTransfer(QObject):

    sig_finished = pyqtSignal()
    sig_close_requested = pyqtSignal()

    def __init__(self, app, action: Callable, sources: List[str], destination: str) -> None:
        super().__init__()

        self._app = app

        thread = QThread()

        mediator = GuiMediator(None)
        mediator.sig_file_conflict.connect(self._on_file_conflict)
        mediator.sig_directory_conflict.connect(self._on_directory_conflict)
        mediator.moveToThread(thread)

        progress = GuiProgress()

        transfer_dialog = TransferDialog(destination, None)
        transfer_dialog.connect(progress)
        transfer_dialog.finished.connect(self._on_finished)
        transfer_dialog.show()

        worker = TransferWorker(self._app.fs, action, sources, destination, mediator, progress)
        worker.moveToThread(thread)
        thread.started.connect(worker.on_started)
        thread.start()

        self.sig_close_requested.connect(worker.close, type=Qt.BlockingQueuedConnection)

        self._thread = thread
        self._worker = worker
        self._transfer_dialog = transfer_dialog

    def close(self) -> None:
        assert self._worker._close is False
        self._worker._close = True
        self.sig_close_requested.emit()
        self._thread.quit()
        self._thread.wait()

    def _on_finished(self, result: int):
        self.sig_finished.emit()

    def _on_file_conflict(self, retval: ReturnValue[Resolution]) -> None:
        dialog = ConflictDialog(None)
        resolution = Resolution(dialog.exec())
        retval.send(resolution)

    def _on_directory_conflict(self, retval: ReturnValue[Resolution]) -> None:
        dialog = ConflictDialog(None)
        resolution = Resolution(dialog.exec())
        retval.send(resolution)


class FilesystemOperations:

    """Filesystem operations for the GUI. Messageboxes and dialogs will be
    shown on errors and conflicts."""

    def __init__(self, app) -> None:
        self._app = app
        self._rename_op = RenameOperation()
        self._transfers: List[GuiFileTransfer] = []

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
        transfer = GuiFileTransfer(self._app, action, sources, destination)
        transfer.sig_finished.connect(lambda transfer=transfer: self._cleanup_transfer(transfer))
        self._transfers.append(transfer)

    def _cleanup_transfer(self, transfer: GuiFileTransfer) -> None:
        transfer.close()
        self._transfers.remove(transfer)

    def create_file(self, path: str) -> None:
        self._app.fs.create_file(path)

    def create_directory(self, path: str) -> None:
        self._app.fs.create_directory(path)


# EOF #
