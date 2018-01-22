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


import gc
import types
import signal
import sys
import dbus
from PyQt5.QtCore import (Qt, QCoreApplication, QThread, QObject,
                          pyqtSlot, pyqtSignal, QTimer, QAbstractEventDispatcher, QEventLoop)
from dbus.mainloop.pyqt5 import DBusQtMainLoop

from dirtools.dbus_thumbnailer import DBusThumbnailer


class Worker(QObject):

    sig_finished = pyqtSignal()
    sig_thing_finished = pyqtSignal(str, types.FunctionType)

    def __init__(self, name):
        super().__init__()
        self.quit = False
        self.name = name

    def init(self):
        print("Worker.init:", QThread.currentThread())
        # Delay creation so we are in the correct thread
        self.dbus_loop = DBusQtMainLoop(set_as_default=False)
        self.session_bus = dbus.SessionBus(mainloop=self.dbus_loop)
        self.dbus_thumbnailer = DBusThumbnailer(self.session_bus)

    def __del__(self):
        print("__del__")

    def deinit(self):
        assert self.quit == True

        print(self.name, "Worker.deinit --------------------")

        self.session_bus.close()
        del self.dbus_thumbnailer
        del self.session_bus
        del self.dbus_loop

        self.sig_finished.emit()

    def on_thing_requested(self, thing, callback):
        if self.quit:
            self.sig_thing_finished.emit("aborted-" + thing + "-Work", callback)
            return

        QThread.sleep(1)
        self.sig_thing_finished.emit(thing + "-Work", callback)


class WorkerFacade(QObject):

    sig_thing_requested = pyqtSignal(str, types.FunctionType)
    sig_quit_requested = pyqtSignal()

    def __init__(self, name):
        super().__init__()
        self.quit = False
        self.name = name

        self.worker = Worker("worker-" + name)
        self.thread = QThread()
        self.worker.moveToThread(self.thread)

        # startup and shutdown
        self.thread.started.connect(self.worker.init)
        self.sig_quit_requested.connect(self.worker.deinit)
        self.worker.sig_finished.connect(self.thread.quit)

        # requests and receive
        self.worker.sig_thing_finished.connect(self.on_thing_finished)
        self.sig_thing_requested.connect(self.worker.on_thing_requested)

        self.thread.start()

    def request_thing(self, thing, callback):
        self.sig_thing_requested.emit(self.name + "-" + thing, callback)

    def on_thing_finished(self, thing, callback):
        callback(thing)

    def on_quit(self):
        if self.worker.quit: return

        print(self.name, "WorkerFacade.on_quit()")
        # This signal won't be received until all work is done!!!!
        self.sig_quit_requested.emit()

        # Thus notify worker via sidechannel to stop doing it's job
        self.worker.quit = True

        # waiting for the thread to finish
        print(self.name, "evdi waiting")
        evdi = QAbstractEventDispatcher.instance()
        while self.thread.wait(10) == False:
            evdi.processEvents(QEventLoop.AllEvents)
        print(self.name, "evdi waiting: done")


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication(argv)

    print("Starting:", QThread.currentThread())

    fac1 = WorkerFacade("1")
    fac2 = WorkerFacade("2")

    for i in range(10):
        print("Requesting", i)
        fac1.request_thing("Foo" + str(i), lambda x: print("Got:", x))
        fac2.request_thing("Foo" + str(i), lambda x: print("Got:", x))

    def quits():
        print("calling quits")
        fac1.on_quit()
        fac2.on_quit()

    def app_quit():
        print("App quit")
        quits()
        app.quit()

    # Calling QThread.quit() will not terminate the thread until all
    # signals have been processed
    #
    # Or not, calling quits early will cause a segfault when dbus is
    # involved. The requested quit signal isn't getting processed
    QTimer.singleShot(2000, quits)

    # QThread must be all destroyed before the application is
    # quitting, else ugly "Aborted (core dumped)" will result
    QTimer.singleShot(1500, app_quit)

    print("Entering event loop:", QThread.currentThread())
    app.exec()
    print("Cleanly exited")


if __name__ == "__main__":
    main(sys.argv)


# EOF #
