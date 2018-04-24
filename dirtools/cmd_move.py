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


from typing import List, Optional

import argparse
import hashlib
import os
import sys


class MoveContext:

    def __init__(self) -> None:
        self.verbose = False
        self.dry_run = False
        self.overwrite: Optional[str] = None

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

    def resolve_conflict(self, source: str, dest: str) -> str:
        source_sha1 = sha1sum(source)
        dest_sha1 = sha1sum(dest)
        if source == dest:
            print("skipping '{}' same file as '{}'".format(source, dest))
            return "skip"
        elif source_sha1 == dest_sha1:
            print("skipping '{}' same content as '{}'".format(source, dest))
            return "skip"
        else:
            print("Conflict: {}: destination file already exists".format(dest))
            print("source:\n{}\n  sha1: {}".format(file_info(source), source_sha1))
            print("target:\n{}\n  sha1: {}".format(file_info(dest), dest_sha1))
            while True:
                c = input("Overwrite {} ([Y]es, [N]o, [A]lways, n[E]ver)? ".format(dest))  # [R]ename, [Q]uit
                c = c.lower()
                if c == 'n':
                    print("skipping {}".format(source))
                    return "skip"
                elif c == 'y':
                    return "continue"
                elif c == 'a':
                    self.overwrite = 'always'
                    return "continue"
                elif c == 'e':
                    self.overwrite = 'never'
                    return "skip"
                else:
                    pass  # try to read input again


def bytes2human(num_bytes: int) -> str:
    if num_bytes < 1000:
        return "{} B".format(num_bytes)
    elif num_bytes < 1000 * 1000:
        return "{} KB".format(num_bytes)
    elif num_bytes < 1000 * 1000 * 1000:
        return "{} MB".format(num_bytes)
    elif num_bytes < 1000 * 1000 * 1000 * 1000:
        return "{} GB".format(num_bytes)
    else:
        return "{} TB".format(num_bytes)


def file_info(filename: str) -> str:
    return ("  name: {}\n"
            "  size: {}").format(filename,
                                 bytes2human(os.path.getsize(filename)))


def merge_directory(ctx: MoveContext, source: str, destdir: str):
    for name in os.listdir(source):
        path = os.path.join(source, name)
        if os.path.isdir(path):
            move_directory(ctx, path, destdir)
        else:
            move_file(ctx, path, destdir)


def sha1sum(filename: str, blocksize: int=65536) -> str:
    with open(filename, 'rb') as fin:
        hasher = hashlib.sha1()
        buf = fin.read(blocksize)
        while len(buf) > 0:
            hasher.update(buf)
            buf = fin.read(blocksize)
        return hasher.hexdigest()


def move_file(ctx: MoveContext, source: str, destdir: str) -> None:
    base = os.path.basename(source)
    dest = os.path.join(destdir, base)

    if os.path.exists(dest) and ctx.overwrite != "always":
        if ctx.overwrite == "never":
            ctx.skip_rename(source, dest)
        else:
            resolution = ctx.resolve_conflict(source, dest)
            if resolution == "skip":
                ctx.skip_rename(source, dest)
            elif resolution == "continue":
                ctx.rename(source, dest)
            else:
                assert False, "unknown conflict resolution: %r" % resolution
    else:
        ctx.rename(source, dest)


def move_directory(ctx: MoveContext, source: str, destdir: str) -> None:
    base = os.path.basename(source)
    dest = os.path.join(destdir, base)

    if os.path.exists(dest):
        merge_directory(ctx, source, dest)
    else:
        ctx.rename(source, dest)


def move_path(ctx: MoveContext, source: str, destdir: str, prefix: str) -> None:
    if not os.path.isdir(destdir):
        raise Exception("{}: target directory does not exist".format(destdir))

    if prefix is not None:
        ctx.makedirs(os.path.join(destdir, prefix))
        destdir = os.path.join(destdir, prefix)

    if os.path.isdir(source):
        move_directory(ctx, source, destdir)
    else:
        move_file(ctx, source, destdir)


def move_cmd(ctx: MoveContext, source: str, destdir: str, relative: bool) -> None:
    if relative:
        prefix = os.path.dirname(source)
    else:
        prefix = None

    move_path(ctx, source, destdir, prefix)


def move_multi_cmd(ctx: MoveContext, sources: List[str], destdir: str, relative: bool) -> None:
    for source in sources:
        move_cmd(ctx, source, destdir, relative)


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
                        help="Never overwrite any files")
    parser.add_argument('-Y', '--always', action='store_true', default=False,
                        help="Always overwrite files on conflict")
    return parser.parse_args(args)


def main(argv: List[str]) -> None:
    args = parse_args(argv[1:])

    ctx = MoveContext()
    ctx.verbose = args.verbose
    ctx.dry_run = args.dry_run
    if args.always:
        ctx.overwrite = 'always'
    if args.never:
        ctx.overwrite = 'never'

    sources = [os.path.normpath(p) for p in args.FILE]
    destdir = os.path.normpath(args.target_directory)

    move_multi_cmd(ctx, sources, destdir, args.relative)


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
