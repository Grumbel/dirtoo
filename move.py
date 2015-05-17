#!/usr/bin/env python3

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


import argparse
import os


class Context:

    def __init__(self)
        self.verbose = False
        self.dry_run = False

    def rename(self, oldpath, newpath):
        if self.verbose:
            print("{} -> {}".format(oldpath, newpath))

        if not self.dry_run:
            os.rename(oldpath, newpath)

    def makedirs(self, path):
        if self.verbose:
            print("makedirs: {}".format(path))

        if not self.dry_run:
            os.makedirs(path)


def merge_directory(ctx, source, destdir):
    for name in os.listdir(source):
        path = os.path.join(source, name)
        if os.path.isdir(path):
            move_directory(ctx, path, destdir)
        else:
            move_file(ctx, path, destdir)


def move_file(ctx, source, destdir):
    base = os.path.basename(source)
    dest = os.path.join(destdir, base)

    if os.path.exists(dest):
        raise Exception("{}: destination file already exists".format(dest))
    else:
        ctx.rename(source, dest)


def move_directory(ctx, source, destdir):
    base = os.path.basename(source)
    dest = os.path.join(destdir, base)

    if os.path.exists(dest):
        merge_directory(ctx, source, dest)
    else:
        ctx.rename(source, dest)


def move_path(ctx, source, destdir, prefix):
    if not os.path.isdir(destdir):
        raise Exception("{}: target directory does not exist".format(destdir))

    if prefix is not None:
        ctx.makedirs(os.path.join(destdir, prefix))
        destdir = os.path.join(destdir, prefix)

    if os.path.isdir(source):
        move_directory(ctx, source, destdir)
    else:
        move_file(ctx, source, destdir)


def move_cmd(ctx, source, destdir, relative):
    if relative:
        prefix = os.path.dirname(source)
    else:
        prefix = None

    move_path(ctx, source, destdir, prefix)


def move_multi_cmd(ctx, sources, destdir, relative):
    for source in sources:
        move_cmd(ctx, source, destdir, relative)


def parse_args():
    parser = argparse.ArgumentParser(description='move')
    parser.add_argument('FILE', action='store', nargs='+',
                        help='Files to move')
    parser.add_argument('-t', '--target-directory', metavar='DIRECTORY', required=True,
                        help="Target directory")
    parser.add_argument('-R', '--relative', action='store_true', default=False,
                        help="Preserve the path prefix on move")
    parser.add_argument('-n', '--dry-run', action='store_true', default=False,
                        help="Don't do anything")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    return parser.parse_args()


def main():
    args = parse_args()

    ctx = Context()
    ctx.verbose = args.verbose
    ctx.dry_run = args.dry_run

    sources = [os.path.normpath(p) for p in args.FILE]
    destdir = os.path.normpath(args.target_directory)

    move_multi_cmd(ctx, sources, destdir, args.relative)


if __name__ == "__main__":
    main()

# EOF #
