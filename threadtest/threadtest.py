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
from PyQt5.QtCore import Qt, QCoreApplication, QThread, QObject, pyqtSlot, pyqtSignal


class Worker(QObject):

    sig_finished = pyqtSignal()

    def __init__(self, name):
        super().__init__()
        self.quit = False
        self.name = name

    def __del__(self):
        print("__del__")

    def on_started(self):
        print("OnStarted")
        i = 0
        while not self.quit:
            QThread.sleep(1)
            print(self.name, ": Do Work: ",  QThread.currentThread())
            i += 1
            if i > 3:
                break
        self.sig_finished.emit()

    def on_quit(self):
        self.quit = True


threads = []


def launch_thread(name):
    thread = QThread()
    worker = Worker(name)
    worker.moveToThread(thread)
    thread.started.connect(worker.on_started)
    worker.sig_finished.connect(thread.quit)

    # This doesn't seem necessary, worker and threads are owned on the
    # Python side:
    #
    # worker.sig_finished.connect(worker.deleteLater)
    # thread.finished.connect(thread.deleteLater)

    thread.start()

    # keep a reference around so they don't get deleted
    threads.append((worker, thread))


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication(argv)

    launch_thread("1")
    launch_thread("2")
    launch_thread("3")

    QThread.sleep(1)
    print("Entering event loop:", QThread.currentThread())
    app.exec()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
