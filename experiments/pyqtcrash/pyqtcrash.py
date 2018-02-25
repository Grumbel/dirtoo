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

from PyQt5.QtCore import QCoreApplication, QTimer, QObject, pyqtSignal


# The program crashes when 'main()' is called twice. The cause for the
# crash seems to be that 'sig_test' is connected to a temporary or
# local function that contains a binding to 'app. If the temporary is
# replaced with a function at module scope or is kept alive by
# assigning it to a variable of a surrounding scope everything seems
# to work fine.

# flake8: noqa

# Program received signal SIGSEGV, Segmentation fault.
# __memmove_ssse3 () at ../sysdeps/x86_64/multiarch/memcpy-ssse3.S:2831
# 2831	../sysdeps/x86_64/multiarch/memcpy-ssse3.S: No such file or directory.
# (gdb) where
# #0  __memmove_ssse3 () at ../sysdeps/x86_64/multiarch/memcpy-ssse3.S:2831
# #1  0x00007ffff5cae790 in ?? () from /usr/lib/x86_64-linux-gnu/libQt5Core.so.5
# #2  0x00007ffff5ca9755 in QCoreApplicationPrivate::sendPostedEvents(QObject*, int, QThreadData*) () from /usr/lib/x86_64-linux-gnu/libQt5Core.so.5
# #3  0x00007ffff5cffe53 in ?? () from /usr/lib/x86_64-linux-gnu/libQt5Core.so.5
# #4  0x00007ffff49a3fb7 in g_main_context_dispatch () from /lib/x86_64-linux-gnu/libglib-2.0.so.0
# #5  0x00007ffff49a41f0 in ?? () from /lib/x86_64-linux-gnu/libglib-2.0.so.0
# #6  0x00007ffff49a427c in g_main_context_iteration () from /lib/x86_64-linux-gnu/libglib-2.0.so.0
# #7  0x00007ffff5cff47f in QEventDispatcherGlib::processEvents(QFlags<QEventLoop::ProcessEventsFlag>) () from /usr/lib/x86_64-linux-gnu/libQt5Core.so.5
# #8  0x00007ffff5ca4e3a in QEventLoop::exec(QFlags<QEventLoop::ProcessEventsFlag>) () from /usr/lib/x86_64-linux-gnu/libQt5Core.so.5
# #9  0x00007ffff5cadda4 in QCoreApplication::exec() () from /usr/lib/x86_64-linux-gnu/libQt5Core.so.5
# #10 0x00007ffff6349a1b in meth_QCoreApplication_exec (sipArgs=<optimized out>) at ./build-3.6/QtCore/sipQtCorepart7.cpp:16763
# #11 0x00000000004c4a3d in _PyCFunction_FastCallDict (kwargs=0x0, nargs=140737327578856, args=0xad3500, func_obj=<built-in method exec of QCoreApplication object at remote 0x7ffff66abee8>) at ../Objects/methodobject.c:234
# #12 _PyCFunction_FastCallKeywords (func=func@entry=<built-in method exec of QCoreApplication object at remote 0x7ffff66abee8>, stack=stack@entry=0xad3500, nargs=nargs@entry=0, kwnames=kwnames@entry=0x0) at ../Objects/methodobject.c:294
# #13 0x000000000054f3c4 in call_function (pp_stack=pp_stack@entry=0x7fffffffcfb8, oparg=<optimized out>, kwnames=kwnames@entry=0x0) at ../Python/ceval.c:4824
# #14 0x0000000000553aaf in _PyEval_EvalFrameDefault (f=<optimized out>, throwflag=<optimized out>) at ../Python/ceval.c:3322
# #15 0x000000000054efc1 in PyEval_EvalFrameEx (throwflag=0, f=Python Exception <class 'RuntimeError'> Type does not have a target.:
# ) at ../Python/ceval.c:753
# #16 _PyEval_EvalCodeWithName (_co=<code at remote 0x7ffff6703b70>, Python Exception <class 'RuntimeError'> Type does not have a target.:
# globals=globals@entry=, locals=locals@entry=0x0, args=<optimized out>, argcount=argcount@entry=0, kwnames=0x0, kwargs=0xae9628, kwcount=0, kwstep=1, defs=0x0, defcount=0, kwdefs=0x0,
#     closure=0x0, Python Exception <class 'RuntimeError'> Type does not have a target.:
# name=, Python Exception <class 'RuntimeError'> Type does not have a target.:
# qualname=) at ../Python/ceval.c:4153


class Testo(QObject):

    sig_test = pyqtSignal(str)


def main():
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QCoreApplication([])

    testo = Testo()

    # This is causing the problem
    def a():
        # Using 'app' in a local function seems to be what requires
        # the manual 'del app' call, otherwise the garbage collector
        # needs to do the cleanup and probably comes to late, as
        # another QCoreApplication instance might have already been
        # created.
        print(app)

    # As long as the function isn't using 'app' everything seems to be
    # ok.
    def b():
        print("nothing")

    # If 'b' is used instead of 'a' the problem goes away
    testo.sig_test.connect(a)

    QTimer.singleShot(1000, app.quit)

    print("exec")
    app.exec()
    print("exec:done")

    # Neither of these fix the issue:
    #
    # testo.sig_test.disconnect(a)
    # del testo

    # This solves the crash:
    # del app

    # This however doesn't help:
    # del a

if __name__ == "__main__":
    main()
    main()


# EOF #
