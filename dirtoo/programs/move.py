# dirtoo - File and directory manipulation tools for Python
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
import os
import sys

from dirtoo.file_transfer import FileTransfer, ConsoleMediator, ConsoleProgress, Overwrite
from dirtoo.filesystem import Filesystem


def parse_args(action: str, args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="{} files and directories.".format(action.capitalize()))

    parser.add_argument('FILE', action='store', nargs='+',
                        help='Files to {}'.format(action))
    parser.add_argument('-t', '--target-directory', metavar='DIRECTORY', required=True,
                        help="Target directory")
    parser.add_argument('-R', '--relative', action='store_true', default=False,
                        help="Preserve the path prefix on {}".format(action))
    parser.add_argument('-n', '--dry-run', action='store_true', default=False,
                        help="Don't do anything, just print the actions")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    parser.add_argument('-N', '--never', action='store_true', default=False,
                        help="NEVER overwrite any files")
    parser.add_argument('-Y', '--always', action='store_true', default=False,
                        help="ALWAYS overwrite files on conflict")
    return parser.parse_args(args)


def main(action: str, argv: List[str]) -> None:
    args = parse_args(action, argv[1:])

    sources = [os.path.normpath(p) for p in args.FILE]
    destdir = os.path.normpath(args.target_directory)

    fs = Filesystem()
    fs.verbose = args.verbose
    fs.enabled = not args.dry_run

    mediator = ConsoleMediator()
    if args.always:
        mediator.overwrite = Overwrite.ALWAYS
    if args.never:
        mediator.overwrite = Overwrite.NEVER

    progress = ConsoleProgress()
    progress.verbose = args.verbose

    if not fs.isdir(destdir):
        raise Exception("{}: target directory does not exist".format(destdir))

    ctx = FileTransfer(fs, mediator, progress)
    for source in sources:
        if args.relative:
            actual_destdir = ctx.make_relative_dir(source, destdir)
        else:
            actual_destdir = destdir

        if action == "copy":
            assert args.relative is False
            ctx.copy(source, actual_destdir)
        elif action == "move":
            ctx.move(source, actual_destdir)


def move_main_entrypoint() -> None:
    main("move", sys.argv)


def copy_main_entrypoint() -> None:
    main("copy", sys.argv)


# EOF #
