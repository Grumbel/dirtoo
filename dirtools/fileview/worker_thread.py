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


from PyQt5.QtCore import QObject, pyqtSignal, Qt, QThread


class Worker(QObject):

    def __init__(self):
        super().__init__()

        self._close = False

    def on_thread_started(self) -> None:
        pass

    def close(self) -> None:
        pass


class WorkerThread(QObject):

    sig_close_requested = pyqtSignal()

    def __init__(self, worker) -> None:
        super().__init__()

        self._worker = worker
        self._thread = QThread()
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.on_thread_started)

        # close() is a blocking connection so the thread is properly
        # done after the signal was emit'ed and we don't have to fuss
        # around with sig_finished() and other stuff
        self.sig_close_requested.connect(self._worker.close, type=Qt.BlockingQueuedConnection)

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        assert self._worker._close is False, "WorkerThread.close() was called twice"
        self._worker._close = True
        self.sig_close_requested.emit()
        self._thread.quit()
        self._thread.wait()


# EOF #
