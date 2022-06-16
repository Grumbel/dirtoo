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


from typing import Dict, List, Any

import traceback
import logging
import os

from PyQt5.QtCore import QObject, pyqtSignal, QThread
from PyQt5.QtCore import QMimeDatabase

from dirtoo.fileview.metadata_cache import MetaDataCache
from dirtoo.fileview.metadata import MetaData
from dirtoo.fileview.stdio_filesystem import StdioFilesystem
from dirtoo.location import Location

logger = logging.getLogger(__name__)


class MetaDataCollectorWorker(QObject):

    sig_metadata_ready = pyqtSignal(Location, dict)

    def __init__(self, vfs: StdioFilesystem) -> None:
        super().__init__()

        self.vfs = vfs
        self._close = False

    def init(self):
        self.mimedb = QMimeDatabase()
        self.cache = MetaDataCache()

    def on_delete_metadata_requested(self, locations: List[Location]) -> None:
        for location in locations:
            abspath = self.vfs.get_stdio_name(location)
            self.cache.clear_metadata(abspath)

    def on_metadata_requested(self, location: Location) -> None:
        if self._close:
            return

        abspath = self.vfs.get_stdio_name(location)
        cached_metadata = self.cache.retrieve_metadata(abspath)

        try:
            stat = os.lstat(abspath)
            metadata: Dict[str, Any] = {}

        except FileNotFoundError as err:
            error_message = "".join(traceback.format_exception(etype=type(err),
                                                               value=err,
                                                               tb=err.__traceback__))
            metadata = {
                'location': location.as_url(),
                'path': abspath,
                'mtime': 0,
                'type': "error",
                'error_message': error_message
            }
            self.sig_metadata_ready.emit(location, metadata)

        else:
            if cached_metadata is not None and \
               (("mtime" in cached_metadata) and (stat.st_mtime == cached_metadata["mtime"])):
                self.sig_metadata_ready.emit(location, cached_metadata)
            else:
                try:
                    metadata.update(self._create_generic_metadata(location, abspath))
                    metadata.update(self._create_type_specific_metadata(location, abspath))
                except Exception as err:
                    error_message = "".join(traceback.format_exception(etype=type(err),
                                                                       value=err,
                                                                       tb=err.__traceback__))
                    metadata = {
                        'location': location.as_url(),
                        'path': abspath,
                        'mtime': stat.st_mtime,
                        'type': "error",
                        'error_message': error_message
                    }

                self.cache.store_metadata(abspath, metadata)
                self.sig_metadata_ready.emit(location, metadata)

    def _create_generic_metadata(self, location: Location, abspath: str) -> Dict[str, Any]:
        metadata: Dict[str, Any] = {}

        metadata['location'] = location.as_url()
        metadata['path'] = abspath

        stat = os.lstat(abspath)
        metadata['mtime'] = stat.st_mtime

        return metadata

    def _create_type_specific_metadata(self, location: Location, abspath: str) -> Dict[str, Any]:
        logger.debug("MetaDataCollectorWorker.create_metadata: %s", abspath)
        return MetaData.from_path(abspath, self.mimedb)


class MetaDataCollector(QObject):

    sig_metadata_ready = pyqtSignal(Location, dict)
    sig_request_metadata = pyqtSignal(Location)
    sig_delete_metadatas = pyqtSignal(list)

    def __init__(self, vfs: StdioFilesystem) -> None:
        super().__init__()

        self._worker = MetaDataCollectorWorker(vfs)
        self._thread = QThread(self)
        self._worker.moveToThread(self._thread)

        self._thread.started.connect(self._worker.init)
        self.sig_request_metadata.connect(self._worker.on_metadata_requested)
        self.sig_delete_metadatas.connect(self._worker.on_delete_metadata_requested)
        self._worker.sig_metadata_ready.connect(self._on_metadata_ready)

        self._thread.start()

    def close(self):
        self._worker._close = True
        self._thread.quit()
        self._thread.wait()

    def request_metadata(self, location: Location) -> None:
        self.sig_request_metadata.emit(location)

    def request_delete_metadatas(self, locations: List[Location]) -> None:
        self.sig_delete_metadatas.emit(locations)

    def _on_metadata_ready(self, location: Location, metadata: Any):
        self.sig_metadata_ready.emit(location, metadata)


# EOF #
