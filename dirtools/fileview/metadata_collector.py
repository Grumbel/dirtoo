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

import traceback
import logging
import os

from PyQt5.QtCore import QObject, pyqtSignal, QThread
from PyQt5.QtCore import QMimeDatabase

from PyPDF2 import PdfFileReader

from dirtools.mediainfo import MediaInfo
from dirtools.archiveinfo import ArchiveInfo
from dirtools.fileview.metadata_cache import MetaDataCache


class MetaDataCollectorWorker(QObject):

    sig_metadata_ready = pyqtSignal(str, dict)

    def __init__(self) -> None:
        super().__init__()
        self._close = False

    def init(self):
        self.mimedb = QMimeDatabase()
        self.cache = MetaDataCache()

    def on_metadata_requested(self, filename: str):
        if self._close:
            return

        abspath = os.path.abspath(filename)
        cached_metadata = self.cache.retrieve_metadata(abspath)
        if cached_metadata is not None:
            self.sig_metadata_ready.emit(filename, cached_metadata)
        else:
            try:
                metadata = self._create_metadata(filename)
            except Exception as err:
                error_message = "".join(traceback.format_exception(etype=type(err),
                                                                   value=err,
                                                                   tb=err.__traceback__))
                metadata = {
                    'type': "error",
                    'error_message': error_message
                }

            self.cache.store_metadata(abspath, metadata)
            self.sig_metadata_ready.emit(filename, metadata)

    def _create_metadata(self, filename: str):
        logging.debug("MetaDataCollectorWorker.create_metadata: %s", filename)
        mimetype = self.mimedb.mimeTypeForFile(filename)

        metadata: Dict[str, Any] = {}

        if mimetype.name().startswith("video/"):
            minfo = MediaInfo(filename)
            metadata['type'] = 'video'
            metadata['width'] = minfo.width()
            metadata['height'] = minfo.height()
            metadata['duration'] = minfo.duration()
            metadata['framerate'] = minfo.framerate()
        elif mimetype.name().startswith("audio/"):
            minfo = MediaInfo(filename)
            metadata['type'] = 'audio'
            metadata['duration'] = minfo.duration()
            # bitrate, samplerate, channels
        elif mimetype.name().startswith("image/"):
            metadata['type'] = 'image'
            minfo = MediaInfo(filename)
            metadata['width'] = minfo.width()
            metadata['height'] = minfo.height()
        elif mimetype.name() == 'application/pdf':
            metadata['type'] = 'pdf'
            with open(filename, 'rb') as fin:
                pdf = PdfFileReader(fin)
                metadata['pages'] = pdf.getNumPages()
        elif mimetype.name() in ['application/zip',
                                 'application/vnd.rar',
                                 'application/rar']:
            archiveinfo = ArchiveInfo.from_file(filename)
            metadata['type'] = 'archive'
            metadata['file_count'] = archiveinfo.file_count
        elif mimetype.name() == "inode/directory":
            metadata['type'] = 'directory'
            entries = os.listdir(filename)
            metadata['file_count'] = len(entries)
        else:
            logging.debug("MetaDataCollectorWorker.on_metadata_requested: "
                          "unhandled mime-type: %s - %s",
                          filename, mimetype.name())

        return metadata


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
