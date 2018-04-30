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

import os
import shutil
import stat


class Filesystem:
    """Low level filesystem functions, unlike the standard POSIX function
    the functions here try to be non-destructive and will error out when
    trying to overwrite files unless an overwrite is explicitly
    requested."""

    def __init__(self) -> None:
        self.verbose: bool = False
        self.dry_run: bool = False

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
        os.symlink(src, dst)

    def mkdir(self, path: str) -> None:
        os.mkdir(path)

    def rmdir(self, path: str) -> None:
        os.rmdir(path)

    def remove_file(self, path: str):
        if self.verbose or self.dry_run:
            print("remove_file {}".format(path))

        if not self.dry_run:
            os.unlink(path)

    def overwrite(self, src: str, dst: str) -> None:
        if self.verbose or self.dry_run:
            print("overwriting {} -> {}".format(src, dst))

        if not self.dry_run:
            os.rename(src, dst)

    def rename(self, oldpath: str, newpath: str) -> None:
        if self.verbose or self.dry_run:
            print("rename {} -> {}".format(oldpath, newpath))

        if not self.dry_run:
            if os.path.lexists(newpath):
                raise FileExistsError(newpath)
            else:
                os.rename(oldpath, newpath)

    def copy_stat(self, src: str, dst: str) -> None:
        if not self.dry_run:
            shutil.copystat(src, dst, follow_symlinks=False)

    def copy_filecontent(self, src: str, dst: str, progress_cb: Callable[[int, int], None], buf_size: int=16 * 1024):
        if self.dry_run:
            return

        if os.path.lexists(dst):
            raise FileExistsError(dst)

        with open(src, 'rb') as fd_src, open(dst, 'wb') as fd_dst:

            if progress_cb is not None:
                fd_src.seek(0, os.SEEK_END)
                total_size = fd_src.tell()
                current_size = 0

            while True:
                buf = fd_src.read(buf_size)
                if not buf:
                    break

                fd_dst.write(buf)
                if progress_cb is not None:
                    current_size += len(buf)
                    progress_cb(current_size, total_size)

    def copy_file(self, src: str, dst: str, overwrite: bool=False) -> None:
        st = os.stat(src)
        if not (stat.S_ISREG(st.st_mode) or
                stat.S_ISLNK(st.st_mode)):
            raise Exception("unknown filetype: {}".format(src))
            # stat.S_ISDIR(mode)
            # stat.S_ISCHR(mode)
            # stat.S_ISBLK(mode)
            # stat.S_ISFIFO(mode)
            # stat.S_ISSOCK(mode)

        if not self.dry_run:
            if not overwrite and os.path.lexists(dst):
                raise FileExistsError(dst)
            else:
                # Will throw "shutil.SameFileError"
                shutil.copy2(src, dst, follow_symlinks=False)

    def makedirs(self, path: str) -> None:
        if self.verbose:
            print("makedirs: {}".format(path))

        if not self.dry_run:
            # makedirs() fails if the last element in the path already exists
            if not os.path.isdir(path):
                os.makedirs(path)


# EOF #
