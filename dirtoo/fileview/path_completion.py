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


from typing import List, Tuple

import os

from PyQt5.QtCore import pyqtSignal

from dirtoo.fileview.worker_thread import WorkerThread, Worker


class PathCompletionWorker(Worker):

    sig_completions_ready = pyqtSignal(str, list)

    def __init__(self) -> None:
        super().__init__()

        self._request_interrupted = False

    def _on_request_completions(self, text: str) -> None:
        self._request_interrupted = False
        longest, candidates = self.complete(text)
        self.sig_completions_ready.emit(longest, candidates)

    def candidates(self, text: str) -> List[str]:
        dirname = os.path.dirname(text)
        basename = os.path.basename(text)

        candidates = []

        try:
            for entry in os.scandir(dirname):
                if self._close or self._request_interrupted:
                    return []

                if entry.is_dir():
                    if entry.name.startswith(basename):
                        candidates.append(entry.path)
        except OSError:
            pass

        return sorted(candidates)

    def complete(self, text: str) -> Tuple[str, List[str]]:
        candidates = self.candidates(text)

        if candidates == []:
            longest = text
        else:
            longest = os.path.commonprefix(candidates)

        return longest, candidates


class PathCompletion(WorkerThread):

    sig_request_completions = pyqtSignal(str)

    def __init__(self) -> None:
        super().__init__()
        self.set_worker(PathCompletionWorker())
        assert self._worker is not None
        self.sig_request_completions.connect(self._worker._on_request_completions)

    def request_completions(self, text: str) -> None:
        self._request_interrupted = True
        self.sig_request_completions.emit(text)

    @property
    def sig_completions_ready(self) -> pyqtSignal:
        assert self._worker is not None
        return self._worker.sig_completions_ready


# EOF #
