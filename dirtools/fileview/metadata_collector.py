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


from typing import Dict, Any

from PyQt5.QtCore import QObject, pyqtSignal, QThread
from PyQt5.QtCore import QMimeDatabase

from dirtools.mediainfo import MediaInfo


class MetaDataCollectorWorker(QObject):

    sig_metadata_ready = pyqtSignal(str, dict)

    def __init__(self) -> None:
        super().__init__()
        self._close = False

    def init(self):
        self.mimedb = QMimeDatabase()

    def on_metadata_requested(self, filename: str):
        if self._close:
            return

        print("MetaDataCollectorWorker.processing: {}".format(filename))
        mimetype = self.mimedb.mimeTypeForFile(filename)

        metadata: Dict[str, Any] = {}

        if mimetype.name().startswith("video/"):
            minfo = MediaInfo(filename)
            metadata['type'] = 'video'
            metadata['width'] = minfo.width()
            metadata['height'] = minfo.height()
            metadata['duration'] = minfo.duration()
            metadata['framerate'] = minfo.framerate()
        elif mimetype.name().startswith("image/"):
            metadata['type'] = 'image'
            minfo = MediaInfo(filename)
            metadata['width'] = minfo.width()
            metadata['height'] = minfo.height()
        else:
            pass

        self.sig_metadata_ready.emit(filename, metadata)


class MetaDataCollector(QObject):

    sig_metadata_ready = pyqtSignal(str, dict)
    sig_request_metadata = pyqtSignal(str)

    def __init__(self) -> None:
        super().__init__()

        self.worker = MetaDataCollectorWorker()
        self.thread = QThread()
        self.worker.moveToThread(self.thread)

        self.thread.started.connect(self.worker.init)
        self.sig_request_metadata.connect(self.worker.on_metadata_requested)
        self.worker.sig_metadata_ready.connect(self._on_metadata_ready)

        self.thread.start()

    def close(self):
        self.worker._close = True
        self.thread.quit()
        self.thread.wait()

    def request_metadata(self, filename: str) -> None:
        self.sig_request_metadata.emit(filename)

    def _on_metadata_ready(self, filename, metadata):
        self.sig_metadata_ready.emit(filename, metadata)


# EOF #
