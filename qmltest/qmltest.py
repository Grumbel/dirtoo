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

import sys
import os
import signal
import urllib.parse
import hashlib
import xdg.BaseDirectory

from PyQt5.QtCore import Qt, QUrl, QByteArray, QVariant, QObject, pyqtProperty
from PyQt5.QtGui import QGuiApplication
from PyQt5.QtQuick import QQuickView, QQuickItem
from PyQt5.QtQml import QQmlComponent


class Foo(QObject):

    def __init__(self, filename, mtime):
        QObject.__init__(self)

        self._filename = filename
        self._mtime = mtime

    @pyqtProperty(str, constant=True)
    def filename(self):
        return self._filename

    @pyqtProperty(str, constant=True)
    def mtime(self):
        return self._mtime


def main(argv):
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    qapp = QGuiApplication([])

    view = QQuickView()
    engine = view.engine()

    dataList = []
    for entry in  os.scandir(argv[1]):
        url = "file://" + urllib.parse.quote(os.path.abspath(entry.path))
        digest = hashlib.md5(os.fsencode(url)).hexdigest()
        result = os.path.join(xdg.BaseDirectory.xdg_cache_home, "thumbnails", "normal", digest + ".png")
        dataList.append(Foo(entry.name, result))

    ctxt = engine.rootContext()
    ctxt.setContextProperty("menu2", QVariant(dataList))

    view.setSource(QUrl('main.qml'))
    # view.setResizeMode(QQuickView.SizeRootObjectToView)
    view.resize(1024, 768)
    view.show()

    # component = QQmlComponent(engine)
    # component.setData(b"import QtQuick 2.0\nText { text: \"Hello world!\" }", QUrl())
    # item = component.create()
    # item.setParentItem(view.rootObject())

    qapp.exec()



if __name__ == "__main__":
    main(sys.argv)


# EOF #