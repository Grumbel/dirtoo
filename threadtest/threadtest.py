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


import types
import signal
import sys
from PyQt5.QtCore import Qt, QCoreApplication, QThread, QObject, pyqtSlot, pyqtSignal, QTimer


class Worker(QObject):

    sig_thing_finished = pyqtSignal(str, types.FunctionType)

    def __init__(self, name):
        super().__init__()
        self.quit = False
        self.name = name

    def __del__(self):
        print("__del__")

    def on_thing_requested(self, thing, callback):
        QThread.sleep(1)
        self.sig_thing_finished.emit(thing + "-Work", callback)

    def on_quit(self):
        self.quit = True


class WorkerFacade(QObject):

    sig_thing_requested = pyqtSignal(str, types.FunctionType)

    def __init__(self, name):
        super().__init__()
        self.quit = False
        self.name = name

        self.worker = Worker("worker-" + name)
        self.thread = QThread()
        self.worker.moveToThread(self.thread)

        self.worker.sig_thing_finished.connect(self.on_thing_finished)
        self.sig_thing_requested.connect(self.worker.on_thing_requested)

        self.thread.start()

    def request_thing(self, thing, callback):
        self.sig_thing_requested.emit(self.name + "-" + thing, callback)

    def on_thing_finished(self, thing, callback):
        callback(thing)

    def on_quit(self):
        self.worker.on_quit()
        self.thread.quit()


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication(argv)

    fac1 = WorkerFacade("1")
    fac2 = WorkerFacade("2")

    for i in range(3):
        print("Requesting", i)
        fac1.request_thing("Foo" + str(i), lambda x: print("Got:", x))
        fac2.request_thing("Foo" + str(i), lambda x: print("Got:", x))

    def quits():
        print("calling quits")
        fac1.on_quit()
        fac2.on_quit()

    def app_quit():
        print("App quit")
        app.quit()

    # Calling QThread.quit() will not terminate the thread until all
    # signals have been processed
    QTimer.singleShot(1500, quits)

    # QThread must be all destroyed before the application is
    # quitting, else ugly "Aborted (core dumped)" will result
    QTimer.singleShot(2000, app_quit)

    print("Entering event loop:", QThread.currentThread())
    app.exec()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
