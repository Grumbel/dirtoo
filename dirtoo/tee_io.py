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


from typing import IO, List, Optional, Type, Generic, TypeVar
from types import TracebackType


T = TypeVar('T', bytes, str)


class TeeIO(Generic[T]):
    """TeeIO wraps 'input_fd' and records all data read from it to 'output_fd'"""

    def __init__(self, input_fd: IO[T], output_fd: IO[T]) -> None:
        self._input_fd: IO[T] = input_fd
        self._output_fd: IO[T] = output_fd

    def close(self) -> None:
        self._input_fd.close()
        self._output_fd.close()

    @property
    def closed(self) -> bool:
        return self._input_fd.closed

    def fileno(self) -> int:
        return self._input_fd.fileno()

    def flush(self) -> None:
        self._input_fd.flush()
        self._output_fd.flush()

    def isatty(self) -> bool:
        return self._input_fd.isatty()

    def readable(self) -> bool:
        return self._input_fd.readable()

    def readline(self, size: int = -1) -> T:
        line: T = self._input_fd.readline(size)
        self._output_fd.write(line)
        return line

    def readlines(self, hint: int = -1) -> List[T]:
        lines: List[T] = self._input_fd.readlines(hint)
        for line in lines:
            self._output_fd.write(line)
        return lines

    def read(self, n: int = -1) -> T:
        buf: T = self._input_fd.read(n)
        self._output_fd.write(buf)
        return buf

    def tell(self) -> int:
        return self._input_fd.tell()

    def writable(self) -> bool:
        return False

    def __enter__(self) -> 'TeeIO[T]':
        self._input_fd.__enter__()
        self._output_fd.__enter__()
        return self

    def __exit__(self,  # pylint: disable=useless-return
                 exc_type: Optional[Type[BaseException]],
                 exc_value: Optional[BaseException],
                 traceback: Optional[TracebackType]) -> Optional[bool]:
        self._input_fd.__exit__(exc_type, exc_value, traceback)
        self._output_fd.__exit__(exc_type, exc_value, traceback)
        return None


# EOF #
