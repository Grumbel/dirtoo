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


import errno
import hashlib
import os
import sys

from enum import Enum
import bytefmt

from dirtools.filesystem import Filesystem
from dirtools.format import progressbar


class Resolution(Enum):

    SKIP = 1
    CONTINUE = 2


class Overwrite(Enum):

    ASK = 0
    NEVER = 1
    ALWAYS = 2


def sha1sum(filename: str, blocksize: int=65536) -> str:
    with open(filename, 'rb') as fin:
        hasher = hashlib.sha1()
        buf = fin.read(blocksize)
        while len(buf) > 0:
            hasher.update(buf)
            buf = fin.read(blocksize)
        return hasher.hexdigest()


class Mediator:
    """Whenever a filesystem operation would result in the destruction of data,
    the Mediator is called to decide which action should be taken."""

    def __init__(self) -> None:
        self.overwrite: Overwrite = Overwrite.ASK
        self.merge: Overwrite = Overwrite.ASK

    def file_info(self, filename: str) -> str:
        return ("  name: {}\n"
                "  size: {}").format(filename,
                                     bytefmt.humanize(os.path.getsize(filename)))

    def file_conflict(self, source: str, dest: str) -> Resolution:
        if self.overwrite == Overwrite.ASK:
            return self._file_conflict_interactive(source, dest)
        elif self.overwrite == Overwrite.ALWAYS:
            return Resolution.CONTINUE
        elif self.overwrite == Overwrite.NEVER:
            return Resolution.SKIP
        else:
            assert False

    def _file_conflict_interactive(self, source: str, dest: str) -> Resolution:
        source_sha1 = sha1sum(source)
        dest_sha1 = sha1sum(dest)
        if source == dest:
            print("skipping '{}' same file as '{}'".format(source, dest))
            return Resolution.SKIP
        elif source_sha1 == dest_sha1:
            print("skipping '{}' same content as '{}'".format(source, dest))
            return Resolution.SKIP
        else:
            print("Conflict: {}: destination file already exists".format(dest))
            print("source:\n{}\n  sha1: {}".format(self.file_info(source), source_sha1))
            print("target:\n{}\n  sha1: {}".format(self.file_info(dest), dest_sha1))
            while True:
                c = input("Overwrite {} ([Y]es, [N]o, [A]lways, n[E]ver)? ".format(dest))  # [R]ename, [Q]uit
                c = c.lower()
                if c == 'n':
                    print("skipping {}".format(source))
                    return Resolution.SKIP
                elif c == 'y':
                    return Resolution.CONTINUE
                elif c == 'a':
                    self.overwrite = Overwrite.ALWAYS
                    return Resolution.CONTINUE
                elif c == 'e':
                    self.overwrite = Overwrite.NEVER
                    return Resolution.SKIP
                else:
                    pass  # try to read input again

    def directory_conflict(self, sourcedir: str, destdir: str) -> Resolution:
        if self.merge == Overwrite.ASK:
            return self._directory_conflict_interactive(sourcedir, destdir)
        elif self.merge == Overwrite.ALWAYS:
            return Resolution.CONTINUE
        elif self.merge == Overwrite.NEVER:
            return Resolution.SKIP
        else:
            assert False

    def _directory_conflict_interactive(self, sourcedir: str, destdir: str) -> Resolution:
        print("Conflict: {}: destination directory already exists".format(destdir))
        print("source: {}".format(sourcedir))
        print("target: {}".format(destdir))
        while True:
            c = input("Merge into {} ([Y]es, [N]o, [A]lways, n[E]ver)? ".format(destdir))  # [R]ename, [Q]uit
            c = c.lower()
            if c == 'n':
                print("skipping {}".format(sourcedir))
                return Resolution.SKIP
            elif c == 'y':
                return Resolution.CONTINUE
            elif c == 'a':
                self.merge = Overwrite.ALWAYS
                return Resolution.CONTINUE
            elif c == 'e':
                self.merge = Overwrite.NEVER
                return Resolution.SKIP
            else:
                pass  # try to read input again


class Progress:

    def __init__(self):
        self.verbose: bool = False

    def skip_rename(self, oldpath: str, newpath: str) -> None:
        if self.verbose:
            print("skipping {} -> {}".format(oldpath, newpath))

    def skip_copy(self, src: str, dst: str) -> None:
        if self.verbose:
            print("skipping {} -> {}".format(src, dst))

    def skip_move_directory(self, src: str, dst: str) -> None:
        if self.verbose:
            print("skipping {} -> {}".format(src, dst))

    def copy_file(self, src: str, dst: str) -> None:
        if self.verbose:
            print("copying {} -> {}".format(src, dst))

    def copy_directory(self, src: str, dst: str) -> None:
        if self.verbose:
            print("copying {} -> {}".format(src, dst))

    def remove_file(self, src: str) -> None:
        if self.verbose:
            print("removing {}".format(src))

    def link_file(self, src: str, dst: str) -> None:
        if self.verbose:
            print("linking {} -> {}".format(src, dst))

    def copy_progress(self, current: int, total: int) -> None:
        progress = current / total
        total_width = 50

        if current != total:
            sys.stdout.write("{:3d}% |{}|\r".format(
                int(progress * 100),
                progressbar(total_width, current, total)))
        else:
            sys.stdout.write("       {}\r".format(total_width * " "))


class FileTransfer:

    def __init__(self, fs: Filesystem, mediator: Mediator, progress: Progress) -> None:
        self._fs = fs
        self._mediator = mediator
        self._progress = progress

    def _move_file(self, source: str, destdir: str) -> None:
        assert self._fs.isreg(source) or self._fs.islink(source), "{}: unknown file type".format(source)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        base = os.path.basename(source)
        dest = os.path.join(destdir, base)

        if self._fs.lexists(dest):
            resolution = self._mediator.file_conflict(source, dest)
            if resolution == Resolution.SKIP:
                self._progress.skip_rename(source, dest)
            elif resolution == Resolution.CONTINUE:
                try:
                    self._fs.overwrite(source, dest)
                except OSError as err:
                    if err.errno == errno.EXDEV:
                        self._progress.copy_file(source, dest)
                        self._fs.copy_file(source, dest, overwrite=True, progress=self._progress.copy_progress)

                        self._progress.remove_file(source)
                        self._fs.remove_file(source)
                    else:
                        raise
            else:
                assert False, "unknown conflict resolution: %r" % resolution
        else:
            try:
                self._fs.rename(source, dest)
            except OSError as err:
                if err.errno == errno.EXDEV:
                    self._fs.copy_file(source, dest, progress=self._progress.copy_progress)
                    self._fs.remove_file(source)
                else:
                    raise

    def _move_directory_content(self, sourcedir: str, destdir: str) -> None:
        assert os.path.isdir(sourcedir), "{}: not a directory".format(sourcedir)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        for name in self._fs.listdir(sourcedir):
            src = os.path.join(sourcedir, name)
            # FIXME: this could be speed up by using os.scandir()
            self.move(src, destdir)

    def _move_directory(self, sourcedir: str, destdir: str) -> None:
        assert os.path.isdir(sourcedir), "{}: not a directory".format(sourcedir)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        base = os.path.basename(sourcedir)
        dest = os.path.join(destdir, base)

        if self._fs.lexists(dest):
            resolution = self._mediator.directory_conflict(sourcedir, dest)
            if resolution == Resolution.SKIP:
                self._progress.skip_move_directory(sourcedir, dest)
            elif resolution == Resolution.CONTINUE:
                self._move_directory_content(sourcedir, dest)
            else:
                assert False, "unknown conflict resolution: {}".format(resolution)
        else:
            try:
                self._fs.rename(sourcedir, dest)
            except OSError as err:
                if err.errno == errno.EXDEV:
                    self._fs.mkdir(dest)
                    self._fs.copy_stat(sourcedir, dest)
                    self._move_directory_content(sourcedir, dest)
                    self._fs.rmdir(sourcedir)
                else:
                    raise

    def move(self, source: str, destdir: str) -> None:
        """Move 'source' to the directory 'destdir'. 'source' can be any file
        object or directory.
        """

        if not self._fs.isdir(destdir):
            raise Exception("{}: target directory does not exist".format(destdir))

        if os.path.isdir(source):
            self._move_directory(source, destdir)
        else:
            self._move_file(source, destdir)

    def link(self, source: str, destdir: str) -> None:
        dest = os.path.join(destdir, source)
        self._progress.link_file(source, dest)
        self._fs.symlink(source, dest)

    def _copy_file(self, source: str, destdir: str) -> None:
        assert self._fs.isreg(source) or self._fs.islink(source), "{}: unknown file type".format(source)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        base = os.path.basename(source)
        dest = os.path.join(destdir, base)

        if self._fs.lexists(dest):
            resolution = self._mediator.file_conflict(source, dest)
            if resolution == Resolution.SKIP:
                self._progress.skip_copy(source, dest)
            elif resolution == Resolution.CONTINUE:
                self._progress.copy_file(source, dest)
                self._fs.copy_file(source, dest, overwrite=True, progress=self._progress.copy_progress)
            else:
                assert False, "unknown conflict resolution: {}".format(resolution)
        else:
            self._progress.copy_file(source, dest)
            self._fs.copy_file(source, dest, progress=self._progress.copy_progress)

    def _copy_directory_content(self, sourcedir: str, destdir: str) -> None:
        assert os.path.isdir(sourcedir), "{}: not a directory".format(sourcedir)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        for name in self._fs.listdir(sourcedir):
            src = os.path.join(sourcedir, name)
            # FIXME: this could be speed up by using os.scandir()
            self.copy(src, destdir)

    def _copy_directory(self, sourcedir: str, destdir: str) -> None:
        assert os.path.isdir(sourcedir), "{}: not a directory".format(sourcedir)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        base = os.path.basename(sourcedir)
        dest = os.path.join(destdir, base)

        if self._fs.lexists(dest):
            resolution = self._mediator.directory_conflict(sourcedir, dest)
            if resolution == Resolution.SKIP:
                self._progress.skip_copy(sourcedir, dest)
            elif resolution == Resolution.CONTINUE:
                self._progress.copy_directory(sourcedir, destdir)
                self._copy_directory_content(sourcedir, dest)
            else:
                assert False, "unknown conflict resolution: {}".format(resolution)
        else:
            self._progress.copy_directory(sourcedir, dest)
            self._fs.mkdir(dest)
            self._fs.copy_stat(sourcedir, dest)
            self._copy_directory_content(sourcedir, dest)

    def copy(self, source: str, destdir: str) -> None:
        if not self._fs.isdir(destdir):
            raise Exception("{}: target directory does not exist".format(destdir))

        if os.path.isdir(source):
            self._copy_directory(source, destdir)
        else:
            self._copy_file(source, destdir)

    def make_relative_dir(self, source: str, destdir: str) -> str:
        prefix = os.path.dirname(source)

        if os.path.isabs(prefix):
            prefix = os.path.relpath(prefix, "/")

        actual_destdir = os.path.join(destdir, prefix)

        if not os.path.isdir(actual_destdir):
            self._fs.makedirs(actual_destdir)

        return actual_destdir


# EOF #
