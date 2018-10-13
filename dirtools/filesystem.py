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


from typing import List, Callable

import logging
import os
import shutil
import stat

logger = logging.getLogger(__name__)


CopyProgressCallback = Callable[[int, int], None]


def null_progress(current: int, total: int):
    pass


class Filesystem:
    """Low level filesystem functions, unlike the standard POSIX function
    the functions here try to be non-destructive and will error out when
    trying to overwrite files unless an overwrite is explicitly
    requested."""

    def __init__(self) -> None:
        self.buffer_size: int = 16 * 1024

        self.verbose: bool = True
        self.enabled: bool = True

    def set_enabled(self, value: bool) -> None:
        self.enabled = value

    def _message(self, text: str) -> None:
        if self.verbose:
            print(text)
        else:
            logger.info(text)

    def isdir(self, path: str) -> bool:
        return os.path.isdir(path)

    def isreg(self, path: str) -> bool:
        st = os.lstat(path)
        return stat.S_ISREG(st.st_mode)

    def islink(self, path: str) -> bool:
        return os.path.islink(path)

    def lexists(self, path: str) -> bool:
        return os.path.lexists(path)

    def listdir(self, path: str) -> List[str]:
        return os.listdir(path)

    def scandir(self, path: str):
        return os.scandir(path)

    def symlink(self, src: str, dst: str) -> None:
        self._message("symlink {!r} -> {!r}".format(src, dst))

        if self.enabled:
            os.symlink(src, dst)

    def rmdir(self, path: str) -> None:
        self._message("rmdir {!r}".format(path))

        if self.enabled:
            os.rmdir(path)

    def mkdir(self, path: str) -> None:
        self._message("mkdir {!r}".format(path))

        if self.enabled:
            os.mkdir(path)

    def create_directory(self, path: str) -> None:
        self._message("create_directory {!r}".format(path))

        if self.enabled:
            os.mkdir(path)

    def create_file(self, path: str) -> None:
        self._message("create_file {!r}".format(path))

        if self.enabled:
            with open(path, "xb"):
                pass

    def remove_file(self, path: str):
        self._message("remove_file {!r}".format(path))

        if self.enabled:
            os.unlink(path)

    def overwrite(self, src: str, dst: str) -> None:
        self._message("overwrite {!r} -> {!r}".format(src, dst))

        if self.enabled:
            os.rename(src, dst)

    def rename(self, src: str, dst: str) -> None:
        self._message("rename {!r} -> {!r}".format(src, dst))

        if self.enabled:
            if os.path.lexists(dst):
                raise FileExistsError(dst)
            else:
                os.rename(src, dst)

    def rename_unique(self, path: str) -> str:
        new_path = self.generate_unique(path)
        self.rename(path, new_path)
        return new_path

    def generate_unique(self, path: str) -> str:
        base = os.path.basename(path)
        directory = os.path.dirname(path)

        i = 2
        while True:
            new_base = self._gen_name(base, i)
            new_path = os.path.join(directory, new_base)
            if not os.path.lexists(new_path):
                return new_path
            i += 1

    def _gen_name(self, basename: str, i: int) -> str:
        base, ext = os.path.splitext(basename)
        return "{} ({}){}".format(base, i, ext)

    def copy_stat(self, src: str, dst: str) -> None:
        self._message("copy_stat {!r} -> {!r}".format(src, dst))

        if self.enabled:
            shutil.copystat(src, dst, follow_symlinks=False)

    def _copy_filecontent(self, src: str, dst: str,
                          progress: CopyProgressCallback = null_progress):
        assert self.enabled

        with open(src, 'rb') as fd_src, open(dst, 'wb') as fd_dst:

            if progress is not None:
                fd_src.seek(0, os.SEEK_END)
                total_size = fd_src.tell()
                fd_src.seek(0, os.SEEK_SET)
                current_size = 0

            while True:
                buf = fd_src.read(self.buffer_size)

                if not buf:
                    break

                fd_dst.write(buf)

                if progress is not None:
                    current_size += len(buf)
                    progress(current_size, total_size)

    def copy_file(self, src: str, dst: str,
                  overwrite: bool = False,
                  progress: CopyProgressCallback = null_progress) -> None:
        self._message("copy_file {!r} -> {!r}  overwrite={}".format(src, dst, overwrite))

        if not self.enabled:
            return

        st = os.lstat(src)
        if not (stat.S_ISREG(st.st_mode) or
                stat.S_ISLNK(st.st_mode)):
            raise Exception("unknown filetype: {}".format(src))
            # stat.S_ISDIR(mode)
            # stat.S_ISCHR(mode)
            # stat.S_ISBLK(mode)
            # stat.S_ISFIFO(mode)
            # stat.S_ISSOCK(mode)

        if self.enabled:
            if not overwrite and os.path.lexists(dst):
                raise FileExistsError(dst)
            else:
                if self.islink(dst):
                    os.unlink(dst)

                if self.islink(src):
                    os.symlink(os.readlink(src), dst)
                else:
                    self._copy_filecontent(src, dst, progress)
                    shutil.copystat(src, dst, follow_symlinks=False)

    def makedirs(self, path: str) -> None:
        self._message("makedirs: {!r}".format(path))

        if self.enabled:
            # makedirs() fails if the last element in the path already exists
            if not os.path.isdir(path):
                os.makedirs(path)


# EOF #
