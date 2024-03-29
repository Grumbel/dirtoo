# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import cast, Sequence, Any, TextIO, BinaryIO, Union

import os
import sys
import itertools


def expand_file(f: str, recursive: bool) -> list[str]:
    if os.path.isdir(f):
        if recursive:
            lst = [expand_file(os.path.join(f, x), recursive) for x in os.listdir(f)]
            return [item for sublist in lst for item in sublist]
        else:
            return [os.path.join(f, x) for x in os.listdir(f)]
    else:
        return [f]


def expand_directories(files: Sequence[str], recursive: bool) -> list[str]:
    results: list[str] = []
    for f in files:
        results += expand_file(f, recursive)
    return results


def make_non_existing_filename(path: str, name: str) -> str:
    candidate = name
    for idx in itertools.count(2):
        abspath = os.path.join(path, candidate)
        if not os.path.exists(abspath):
            return candidate
        candidate = "{} ({})".format(name, idx)

    assert False, "never reached"


def _open_text_file(filename: str) -> TextIO:
    if filename == '-':
        return sys.stdin
    else:
        return open(filename)


def _open_binary_file(filename: str) -> BinaryIO:
    if filename == '-':
        return sys.stdin.buffer
    else:
        return open(filename, "rb")


# FIXME: split this into two functions
def read_lines_from_file(filename: str, nul_separated: bool) -> Union[list[str], list[bytes]]:
    if nul_separated:
        with _open_binary_file(filename) as fin:
            content = fin.read()
        lines = content.split(b"\0")

        # split() leaves us with a bogus entry at the end, as
        # lines are terminated by \0, not separated.
        if lines[-1] == b"\0":
            lines.pop()

        return lines
    else:
        with _open_text_file(filename) as fin:
            return fin.read().splitlines()


def read_lines_from_files(filenames: Sequence[str], nul_separated: bool) -> Union[list[str], list[bytes]]:
    """Read lines from filenames similar to 'cat'"""

    stdout_read = False

    if not filenames:
        filenames = ["-"]

    lines: Any = []
    for filename in filenames:
        if filename == '-':
            if stdout_read:
                continue
            stdout_read = True

        lines += read_lines_from_file(filename, nul_separated)

    return cast(Union[list[str], list[bytes]], lines)


# EOF #
