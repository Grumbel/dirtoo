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


from typing import Callable

import signal
import sys

from PyQt5.QtCore import (QCoreApplication, QObject, pyqtSignal,
                          QTimer)


class CallableWrap:

    def __init__(self, func: Callable):
        self.func = func

    def __call__(self, *args):
        return self.func(*args)


class Worker(QObject):

    # sig_doit = pyqtSignal(types.FunctionType)
    # sig_doit = pyqtSignal(types.MethodType)
    sig_doit = pyqtSignal(CallableWrap)

    def __init__(self):
        super().__init__()

        self.sig_doit.connect(self.on_doit)

    def on_doit(self, callback):
        print("ON DOIT")
        callback()

    def doit_callback(self):
        print("CALLBACKdoit")


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    app = QCoreApplication(argv)

    worker = Worker()

    def doit():
        print("DO IT")
        worker.sig_doit.emit(CallableWrap(worker.doit_callback))
        worker.sig_doit.emit(CallableWrap(lambda: print("LAMBDADoit")))

    QTimer.singleShot(1000, doit)

    app.exec()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
