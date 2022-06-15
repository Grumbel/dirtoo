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


from typing import IO, Optional, Tuple, cast

import logging

import sys
import uuid
import os

from dirtoo.tee_io import TeeIO

logger = logging.getLogger(__name__)


class StreamManager:

    def __init__(self, cachedir: str) -> None:
        self._cachedir: str = cachedir
        self._stdin_id: Optional[str] = None

    def get_stdin(self) -> Optional[Tuple[IO, str]]:
        if self._stdin_id is None:
            result = self.record(sys.stdin)
            if result is None:
                return None
            tee_io, stream_id = result
            self._stdin_id = stream_id
            return cast(IO, tee_io), stream_id
        else:
            fd = self.lookup(self._stdin_id)
            if fd is not None:
                return fd, self._stdin_id
            else:
                return None

    def lookup(self, stream_id_text: str) -> Optional[IO]:
        stream_id = uuid.UUID(stream_id_text)
        filename = os.path.join(self._cachedir, str(stream_id))
        try:
            fd = open(filename, "r")
        except Exception:
            logger.exception("failed to open '%s' for reading", filename)
            return None
        else:
            return fd

    def record(self, fd: IO[str]) -> Optional[Tuple[TeeIO, str]]:
        outfile, stream_id = self._make_outfile()
        try:
            fd_out = open(outfile, "w")
        except Exception:
            logger.exception("failed to open '%s' for writing", outfile)
            return None
        else:
            tee_io = TeeIO(fd, fd_out)
            return tee_io, stream_id

    def _make_outfile(self) -> Tuple[str, str]:
        stream_id = uuid.uuid4()
        outfile = os.path.join(self._cachedir, str(stream_id))
        return outfile, str(stream_id)


# EOF #
