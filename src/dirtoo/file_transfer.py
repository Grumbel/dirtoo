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


import errno
import hashlib
import os
import sys

from enum import Enum
from abc import ABC, abstractmethod
import bytefmt

from dirtoo.posix.filesystem import Filesystem
from dirtoo.format import progressbar


class CancellationException(Exception):
    pass


class ConflictResolution(Enum):

    CANCEL = 0  # QDialog.Rejected
    OVERWRITE = 1  # QDialog.Accepted
    SKIP = 2
    RENAME_SOURCE = 3
    RENAME_TARGET = 4
    NO_CONFLICT = 5


class Overwrite(Enum):

    ASK = 0
    NEVER = 1
    ALWAYS = 2


def sha1sum(filename: str, blocksize: int = 65536) -> str:
    with open(filename, 'rb') as fin:
        hasher = hashlib.sha1()
        buf = fin.read(blocksize)
        while len(buf) > 0:
            hasher.update(buf)
            buf = fin.read(blocksize)
        return hasher.hexdigest()


class Mediator(ABC):
    """Whenever a filesystem operation would result in the destruction of data,
    the Mediator is called to decide which action should be taken."""

    @abstractmethod
    def file_conflict(self, source: str, dest: str) -> ConflictResolution:
        pass

    @abstractmethod
    def directory_conflict(self, sourcedir: str, destdir: str) -> ConflictResolution:
        pass

    @abstractmethod
    def cancel_transfer(self) -> bool:
        pass


class ConsoleMediator(Mediator):

    def __init__(self) -> None:
        self.overwrite: Overwrite = Overwrite.ASK
        self.merge: Overwrite = Overwrite.ASK

    def cancel_transfer(self) -> bool:
        return False

    def file_info(self, filename: str) -> str:
        return ("  name: {}\n"
                "  size: {}").format(filename,
                                     bytefmt.humanize(os.path.getsize(filename)))

    def file_conflict(self, source: str, dest: str) -> ConflictResolution:
        if self.overwrite == Overwrite.ASK:
            return self._file_conflict_interactive(source, dest)
        elif self.overwrite == Overwrite.ALWAYS:
            return ConflictResolution.OVERWRITE
        elif self.overwrite == Overwrite.NEVER:
            return ConflictResolution.SKIP
        else:
            assert False

    def _file_conflict_interactive(self, source: str, dest: str) -> ConflictResolution:
        source_sha1 = sha1sum(source)
        dest_sha1 = sha1sum(dest)
        if source == dest:
            print("skipping '{}' same file as '{}'".format(source, dest))
            return ConflictResolution.SKIP
        elif source_sha1 == dest_sha1:
            print("skipping '{}' same content as '{}'".format(source, dest))
            return ConflictResolution.SKIP
        else:
            print("Conflict: {}: destination file already exists".format(dest))
            print("source:\n{}\n  sha1: {}".format(self.file_info(source), source_sha1))
            print("target:\n{}\n  sha1: {}".format(self.file_info(dest), dest_sha1))
            while True:
                c = input("Overwrite {} ([Y]es, [N]o, [A]lways, n[E]ver)? ".format(dest))  # [R]ename, [Q]uit
                c = c.lower()
                if c == 'n':
                    print("skipping {}".format(source))
                    return ConflictResolution.SKIP
                elif c == 'y':
                    return ConflictResolution.OVERWRITE
                elif c == 'a':
                    self.overwrite = Overwrite.ALWAYS
                    return ConflictResolution.OVERWRITE
                elif c == 'e':
                    self.overwrite = Overwrite.NEVER
                    return ConflictResolution.SKIP
                else:
                    pass  # try to read input again

    def directory_conflict(self, sourcedir: str, destdir: str) -> ConflictResolution:
        if self.merge == Overwrite.ASK:
            return self._directory_conflict_interactive(sourcedir, destdir)
        elif self.merge == Overwrite.ALWAYS:
            return ConflictResolution.OVERWRITE
        elif self.merge == Overwrite.NEVER:
            return ConflictResolution.SKIP
        else:
            assert False

    def _directory_conflict_interactive(self, sourcedir: str, destdir: str) -> ConflictResolution:
        print("Conflict: {}: destination directory already exists".format(destdir))
        print("source: {}".format(sourcedir))
        print("target: {}".format(destdir))
        while True:
            c = input("Merge into {} ([Y]es, [N]o, [A]lways, n[E]ver)? ".format(destdir))  # [R]ename, [Q]uit
            c = c.lower()
            if c == 'n':
                print("skipping {}".format(sourcedir))
                return ConflictResolution.SKIP
            elif c == 'y':
                return ConflictResolution.OVERWRITE
            elif c == 'a':
                self.merge = Overwrite.ALWAYS
                return ConflictResolution.OVERWRITE
            elif c == 'e':
                self.merge = Overwrite.NEVER
                return ConflictResolution.SKIP
            else:
                pass  # try to read input again


class Progress(ABC):

    @abstractmethod
    def copy_file(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        pass

    @abstractmethod
    def copy_progress(self, current: int, total: int) -> None:
        pass

    @abstractmethod
    def copy_directory(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        pass

    @abstractmethod
    def remove_file(self, src: str) -> None:
        pass

    @abstractmethod
    def remove_directory(self, src: str) -> None:
        pass

    @abstractmethod
    def move_file(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        pass

    @abstractmethod
    def move_directory(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        pass

    @abstractmethod
    def link_file(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        pass

    @abstractmethod
    def transfer_canceled(self) -> None:
        pass

    @abstractmethod
    def transfer_completed(self) -> None:
        pass


class ConsoleProgress(Progress):

    def __init__(self) -> None:
        self.verbose: bool = False

    def copy_file(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        if self.verbose:
            print("copying {} -> {}".format(src, dst))

    def copy_progress(self, current: int, total: int) -> None:
        progress = current / total
        total_width = 50

        if current != total:
            sys.stdout.write("{:3d}% |{}|\r".format(
                int(progress * 100),
                progressbar(total_width, current, total)))
        else:
            sys.stdout.write("       {}\r".format(total_width * " "))

    def copy_directory(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        if self.verbose:
            print("copying {} -> {}".format(src, dst))

    def remove_file(self, src: str) -> None:
        if self.verbose:
            print("removing {}".format(src))

    def remove_directory(self, src: str) -> None:
        if self.verbose:
            print("removing {}".format(src))

    def link_file(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        if self.verbose:
            print("linking {} -> {}".format(src, dst))

    def move_file(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        if self.verbose:
            print("moving {} -> {}".format(src, dst))

    def move_directory(self, src: str, dst: str, resolution: ConflictResolution) -> None:
        if self.verbose:
            print("moving {} -> {}".format(src, dst))

    def transfer_canceled(self) -> None:
        print("transfer canceled")

    def transfer_completed(self) -> None:
        print("transfer completed")


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

        self._progress.move_file(source, dest, ConflictResolution.NO_CONFLICT)
        self._move_file2(source, dest, destdir)

    def _move_file2(self, source: str, dest: str, destdir: str) -> None:
        if self._fs.lexists(dest):
            resolution = self._mediator.file_conflict(source, dest)
            if resolution == ConflictResolution.SKIP:
                self._progress.move_file(source, dest, resolution)
            elif resolution == ConflictResolution.OVERWRITE:
                try:
                    self._fs.overwrite(source, dest)
                except OSError as err:
                    if err.errno == errno.EXDEV:
                        self._progress.copy_file(source, dest, resolution)
                        self._fs.copy_file(source, dest, overwrite=True, progress=self._progress.copy_progress)

                        self._progress.remove_file(source)
                        self._fs.remove_file(source)
                    else:
                        raise
            elif resolution == ConflictResolution.RENAME_SOURCE:
                new_dest = self._fs.generate_unique(dest)
                self._move_file2(source, new_dest, destdir)
            elif resolution == ConflictResolution.RENAME_TARGET:
                self._fs.rename_unique(dest)
                self._move_file(source, destdir)
            elif resolution == ConflictResolution.CANCEL:
                raise CancellationException()
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

        self._progress.move_directory(sourcedir, dest, ConflictResolution.NO_CONFLICT)
        self._move_directory2(sourcedir, dest, destdir)

    def _move_directory2(self, sourcedir: str, dest: str, destdir: str) -> None:
        if self._fs.lexists(dest):
            resolution = self._mediator.directory_conflict(sourcedir, dest)
            if resolution == ConflictResolution.SKIP:
                self._progress.move_directory(sourcedir, dest, resolution)
            elif resolution == ConflictResolution.OVERWRITE:
                self._move_directory_content(sourcedir, dest)
            elif resolution == ConflictResolution.RENAME_SOURCE:
                new_dest = self._fs.generate_unique(dest)
                self._move_directory2(sourcedir, new_dest, destdir)
            elif resolution == ConflictResolution.RENAME_TARGET:
                self._fs.rename_unique(dest)
                self._move_directory(sourcedir, destdir)
            elif resolution == ConflictResolution.CANCEL:
                raise CancellationException()
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

        self.interruption_point()

        if not self._fs.isdir(destdir):
            raise RuntimeError("{}: target directory does not exist".format(destdir))

        if os.path.isdir(source):
            self._move_directory(source, destdir)
        else:
            self._move_file(source, destdir)

    def link(self, source: str, destdir: str) -> None:
        self.interruption_point()

        base = os.path.basename(source)
        dest = os.path.join(destdir, base)

        self._link(source, dest, destdir)

    def _link(self, source: str, dest: str, destdir: str) -> None:
        if self._fs.lexists(dest):
            resolution = self._mediator.file_conflict(source, dest)
            if resolution == ConflictResolution.SKIP:
                self._progress.link_file(source, dest, resolution)
            elif resolution == ConflictResolution.OVERWRITE:
                self._progress.link_file(source, dest, resolution)
                self._fs.remove_file(dest)
                self._fs.symlink(source, dest)
            elif resolution == ConflictResolution.RENAME_SOURCE:
                new_dest = self._fs.generate_unique(dest)
                self._link(source, new_dest, destdir)
            elif resolution == ConflictResolution.RENAME_TARGET:
                self._fs.rename_unique(dest)
                self.link(source, destdir)
            elif resolution == ConflictResolution.CANCEL:
                raise CancellationException()
            else:
                assert False, "unknown conflict resolution: {}".format(resolution)
        else:
            self._progress.link_file(source, dest, ConflictResolution.NO_CONFLICT)
            self._fs.symlink(source, dest)

    def _copy_file(self, source: str, destdir: str) -> None:
        assert self._fs.isreg(source) or self._fs.islink(source), "{}: unknown file type".format(source)
        assert os.path.isdir(destdir), "{}: not a directory".format(destdir)

        base = os.path.basename(source)
        dest = os.path.join(destdir, base)

        self._copy_file2(source, dest, destdir)

    def _copy_file2(self, source: str, dest: str, destdir: str) -> None:
        if self._fs.lexists(dest):
            resolution = self._mediator.file_conflict(source, dest)
            if resolution == ConflictResolution.SKIP:
                self._progress.copy_file(source, dest, resolution)
            elif resolution == ConflictResolution.OVERWRITE:
                self._progress.copy_file(source, dest, resolution)
                self._fs.copy_file(source, dest, overwrite=True, progress=self._progress.copy_progress)
            elif resolution == ConflictResolution.RENAME_SOURCE:
                new_dest = self._fs.generate_unique(dest)
                self._copy_file2(source, new_dest, destdir)
            elif resolution == ConflictResolution.RENAME_TARGET:
                self._fs.rename_unique(dest)
                self._copy_file(source, destdir)
            elif resolution == ConflictResolution.CANCEL:
                raise CancellationException()
            else:
                assert False, "unknown conflict resolution: {}".format(resolution)
        else:
            self._progress.copy_file(source, dest, ConflictResolution.NO_CONFLICT)
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

        self._copy_directory2(sourcedir, dest, destdir)

    def _copy_directory2(self, sourcedir: str, dest: str, destdir: str) -> None:
        if self._fs.lexists(dest):
            resolution = self._mediator.directory_conflict(sourcedir, dest)
            if resolution == ConflictResolution.SKIP:
                self._progress.copy_directory(sourcedir, dest, resolution)
            elif resolution == ConflictResolution.OVERWRITE:
                self._progress.copy_directory(sourcedir, destdir, resolution)
                self._copy_directory_content(sourcedir, dest)
            elif resolution == ConflictResolution.RENAME_SOURCE:
                new_dest = self._fs.generate_unique(dest)
                self._copy_directory2(sourcedir, new_dest, destdir)
            elif resolution == ConflictResolution.RENAME_TARGET:
                self._fs.rename_unique(dest)
                self._copy_directory(sourcedir, destdir)
            elif resolution == ConflictResolution.CANCEL:
                raise CancellationException()
            else:
                assert False, "unknown conflict resolution: {}".format(resolution)
        else:
            self._progress.copy_directory(sourcedir, dest, ConflictResolution.NO_CONFLICT)
            self._fs.mkdir(dest)
            self._fs.copy_stat(sourcedir, dest)
            self._copy_directory_content(sourcedir, dest)

    def copy(self, source: str, destdir: str) -> None:
        self.interruption_point()

        if not self._fs.isdir(destdir):
            raise RuntimeError("{}: target directory does not exist".format(destdir))

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

    def interruption_point(self) -> None:
        if self._mediator.cancel_transfer():
            raise CancellationException()


# EOF #
