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


import os
import signal
import sys
import sys

from PyQt5.QtCore import (QCoreApplication, QFileSystemWatcher,
                          QObject, QSocketNotifier, pyqtSignal, QTimer)
from inotify_simple import INotify, flags as inotify_flags, masks as inotify_masks
import inotify_simple


class INotifyQt(QObject):

    sig_event = pyqtSignal(inotify_simple.Event)

    def __init__(self):
        super().__init__()

        self.inotify = INotify()
        self.qnotifier = QSocketNotifier(self.inotify.fd, QSocketNotifier.Read)
        self.qnotifier.activated.connect(self._on_activated)
        self.wd = None

    def add_watch(self, path, flags=inotify_masks.ALL_EVENTS):
        self.wd = self.inotify.add_watch(path, flags)

    def _on_activated(self, fd):
        assert fd == self.inotify.fd

        for ev in self.inotify.read():
            self.sig_event.emit(ev)

    def close(self):
        del self.qnotifier
        self.inotify.close()


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication([])

    watcher = INotifyQt()

    print("watching /tmp")
    wd = watcher.add_watch('/tmp')

    def on_event(ev):
        print(ev)
        evs = [str(fl) for fl in inotify_flags.from_mask(ev.mask)]
        print("    " + ", ".join(evs))

    watcher.sig_event.connect(on_event)
    # QTimer.singleShot(1000, watcher.close)

    sys.exit(app.exec())


if __name__ == "__main__":
    main(sys.argv)


# EOF #
