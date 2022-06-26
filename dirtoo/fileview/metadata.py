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


from typing import Dict, Any

import os
import logging

from PyPDF2 import PdfFileReader
from PyQt5.QtCore import QMimeDatabase

from dirtoo.mediainfo import MediaInfo
from dirtoo.archive.archiveinfo import ArchiveInfo

logger = logging.getLogger(__name__)


class MetaData:

    @staticmethod
    def from_path(abspath: str, mimedb: QMimeDatabase) -> Dict:
        mimetype = mimedb.mimeTypeForFile(abspath)

        metadata: Dict[str, Any] = {}

        metadata['mime-type'] = mimetype.name()

        if mimetype.name().startswith("video/"):
            minfo = MediaInfo(abspath)
            metadata['type'] = 'video'
            metadata['width'] = minfo.width()
            metadata['height'] = minfo.height()
            metadata['duration'] = minfo.duration()
            metadata['framerate'] = minfo.framerate()
        elif mimetype.name().startswith("audio/"):
            minfo = MediaInfo(abspath)
            metadata['type'] = 'audio'
            metadata['duration'] = minfo.duration()
            # bitrate, samplerate, channels
        elif mimetype.name().startswith("image/"):
            metadata['type'] = 'image'
            minfo = MediaInfo(abspath)
            metadata['width'] = minfo.width()
            metadata['height'] = minfo.height()
        elif mimetype.name() == 'application/pdf':
            metadata['type'] = 'pdf'
            with open(abspath, 'rb') as fin:
                pdf = PdfFileReader(fin)
                metadata['pages'] = pdf.getNumPages()
        elif mimetype.name() in ['application/zip',
                                 'application/vnd.rar',
                                 'application/rar']:
            archiveinfo = ArchiveInfo.from_file(abspath)
            metadata['type'] = 'archive'
            metadata['file_count'] = archiveinfo.file_count
        elif mimetype.name() == "inode/directory":
            metadata['type'] = 'directory'
            entries = os.listdir(abspath)
            metadata['file_count'] = len(entries)
        else:
            logger.debug("MetaDataCollectorWorker.on_metadata_requested: "
                         "unhandled mime-type: %s - %s",
                         abspath, mimetype.name())

        return metadata


# EOF #
