# dirtool.py - diff tool for directories
# Copyright (C) 2015 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import List

import argparse
import hashlib
import os
import sys

from enum import Enum
import bytefmt


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


class Filesystem:

    def __init__(self) -> None:
        self.verbose: bool = False
        self.dry_run: bool = False

    def skip_rename(self, oldpath: str, newpath: str) -> None:
        if self.verbose:
            print("skipping {} -> {}".format(oldpath, newpath))

    def rename(self, oldpath: str, newpath: str) -> None:
        if self.verbose or self.dry_run:
            print("{} -> {}".format(oldpath, newpath))

        if not self.dry_run:
            if os.path.exists(newpath):
                raise FileExistsError(newpath)
            else:
                os.rename(oldpath, newpath)

    def makedirs(self, path: str) -> None:
        if self.verbose:
            print("makedirs: {}".format(path))

        if not self.dry_run:
            # makedirs() fails if the last element in the path already exists
            if not os.path.isdir(path):
                os.makedirs(path)


class Mediator:

    def __init__(self) -> None:
        self.overwrite: Overwrite = Overwrite.ASK

    def file_info(self, filename: str) -> str:
        return ("  name: {}\n"
                "  size: {}").format(filename,
                                     bytefmt.humanize(os.path.getsize(filename)))

    def resolve_conflict(self, source: str, dest: str) -> Resolution:
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


class MoveContext:

    def __init__(self, fs: Filesystem, mediator: Mediator) -> None:
        self.fs = fs
        self.mediator = mediator

    def merge_directory(self, source: str, destdir: str):
        for name in os.listdir(source):
            path = os.path.join(source, name)
            if os.path.isdir(path):
                self.move_directory(path, destdir)
            else:
                self.move_file(path, destdir)

    def move_file(self, source: str, destdir: str) -> None:
        base = os.path.basename(source)
        dest = os.path.join(destdir, base)

        if os.path.exists(dest) and self.mediator.overwrite != Overwrite.ALWAYS:
            if self.mediator.overwrite == Overwrite.NEVER:
                self.fs.skip_rename(source, dest)
            else:
                resolution = self.mediator.resolve_conflict(source, dest)
                if resolution == Resolution.SKIP:
                    self.fs.skip_rename(source, dest)
                elif resolution == Resolution.CONTINUE:
                    self.fs.rename(source, dest)
                else:
                    assert False, "unknown conflict resolution: %r" % resolution
        else:
            self.fs.rename(source, dest)

    def move_directory(self, source: str, destdir: str) -> None:
        base = os.path.basename(source)
        dest = os.path.join(destdir, base)

        if os.path.exists(dest):
            self.merge_directory(source, dest)
        else:
            self.fs.rename(source, dest)

    def move_path(self, source: str, destdir: str, prefix: str) -> None:
        if not os.path.isdir(destdir):
            raise Exception("{}: target directory does not exist".format(destdir))

        if prefix is not None:
            self.fs.makedirs(os.path.join(destdir, prefix))
            destdir = os.path.join(destdir, prefix)

        if os.path.isdir(source):
            self.move_directory(source, destdir)
        else:
            self.move_file(source, destdir)

    def move_cmd(self, source: str, destdir: str, relative: bool) -> None:
        if relative:
            prefix = os.path.dirname(source)
        else:
            prefix = None

        self.move_path(source, destdir, prefix)

    def move_multi_cmd(self, sources: List[str], destdir: str, relative: bool) -> None:
        for source in sources:
            self.move_cmd(source, destdir, relative)


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Move files and directories.")
    parser.add_argument('FILE', action='store', nargs='+',
                        help='Files to move')
    parser.add_argument('-t', '--target-directory', metavar='DIRECTORY', required=True,
                        help="Target directory")
    parser.add_argument('-R', '--relative', action='store_true', default=False,
                        help="Preserve the path prefix on move")
    parser.add_argument('-n', '--dry-run', action='store_true', default=False,
                        help="Don't do anything, just print the actions")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    parser.add_argument('-N', '--never', action='store_true', default=False,
                        help="NEVER overwrite any files")
    parser.add_argument('-Y', '--always', action='store_true', default=False,
                        help="ALWAYS overwrite files on conflict")
    return parser.parse_args(args)


def main(argv: List[str]) -> None:
    args = parse_args(argv[1:])

    sources = [os.path.normpath(p) for p in args.FILE]
    destdir = os.path.normpath(args.target_directory)

    fs = Filesystem()
    fs.verbose = args.verbose
    fs.dry_run = args.dry_run

    mediator = Mediator()
    if args.always:
        mediator.overwrite = Overwrite.ALWAYS
    if args.never:
        mediator.overwrite = Overwrite.NEVER

    ctx = MoveContext(fs, mediator)
    ctx.move_multi_cmd(sources, destdir, args.relative)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
