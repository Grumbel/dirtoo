#!/usr/bin/env python3

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


import signal
import sys
from PyQt5.QtCore import QCoreApplication, QFileSystemWatcher


def directory_changed(path):
    print("directory_changed: {}".format(path))


def file_changed(path):
    print("file_changed: {}".format(path))


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication([])
    watcher = QFileSystemWatcher()

    print("Watching /tmp/")

    watcher.addPath("/tmp/")
    watcher.addPath("/tmp/foo")

    # Files have to be watched specifically for this to trigger.
    # Deleting and recreating a file makes this no longer trigger.
    watcher.fileChanged.connect(file_changed)

    # This triggers on file creation and deletion
    watcher.directoryChanged.connect(directory_changed)

    print("files:", watcher.files())
    print("directories:", watcher.directories())

    sys.exit(app.exec())


if __name__ == "__main__":
    main(sys.argv)


# EOF #
