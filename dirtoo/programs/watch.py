# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018-2022 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import Sequence, Final

import os
import argparse
import signal
import sys
import xdg.BaseDirectory

from PyQt5.QtCore import QCoreApplication

from dirtoo.filesystem.location import Location
from dirtoo.filesystem.file_info import FileInfo
from dirtoo.watcher.directory_watcher import DirectoryWatcher
from dirtoo.filesystem.stdio_filesystem import StdioFilesystem


class DirectoryListener:

    def added(self, fileinfo: FileInfo) -> None:
        print(f"added {fileinfo}")

    def modified(self, fileinfo: FileInfo) -> None:
        print(f"modified {fileinfo}")

    def closed(self, fileinfo: FileInfo) -> None:
        print(f"closed {fileinfo}")

    def removed(self, location: Location) -> None:
        print(f"remove {location}")

    def scandir_finished(self, directory: Sequence[FileInfo]) -> None:
        print("scandir_finished: {}".format('\n  '.join((str(fileinfo) for fileinfo in directory))))

    def message(self, text: str) -> None:
        print(f"message: {text}")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Watch a directory for changes")
    parser.add_argument("DIRECTORY", nargs=1)
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    return parser.parse_args(argv[1:])


def main(argv: Sequence[str]) -> None:
    args = parse_args(argv)

    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication(argv)

    cachedir: Final[str] = os.path.join(xdg.BaseDirectory.xdg_cache_home, "dirtoo")
    stdio_fs: Final[StdioFilesystem] = StdioFilesystem(cachedir)
    location: Final[Location] = Location.from_human(args.DIRECTORY[0])

    watcher: Final[DirectoryWatcher] = DirectoryWatcher(stdio_fs, location)

    print(f"watching {location}")
    watcher.start()

    listener = DirectoryListener()

    watcher.sig_file_added.connect(listener.added)
    watcher.sig_file_modified.connect(listener.modified)
    watcher.sig_file_closed.connect(listener.closed)
    watcher.sig_file_removed.connect(listener.removed)
    watcher.sig_scandir_finished.connect(listener.scandir_finished)
    watcher.sig_message.connect(listener.message)

    app.exec()

    watcher.close()


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
