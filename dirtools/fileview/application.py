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


from typing import List

import signal
import dbus

from PyQt5.QtWidgets import QApplication
from dbus.mainloop.pyqt5 import DBusQtMainLoop

from dirtools.fileview.controller import Controller
from dirtools.fileview.thumbnailer import Thumbnailer
from dirtools.dbus_thumbnail_cache import DBusThumbnailCache
from dirtools.fileview.mime_database import MimeDatabase
from dirtools.fileview.metadata_collector import MetaDataCollector


class FileViewApplication:

    def __init__(self):
        # Allow Ctrl-C killing of the Qt app, see:
        # http://stackoverflow.com/questions/4938723/
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        self.qapp = QApplication([])
        self.thumbnailer = Thumbnailer()
        self.metadata_collector = MetaDataCollector()
        self.dbus_loop = DBusQtMainLoop(set_as_default=False)
        self.session_bus = dbus.SessionBus(self.dbus_loop)
        self.dbus_thumbnail_cache = DBusThumbnailCache(self.session_bus)
        self.mime_database = MimeDatabase()

        self.controllers: List[Controller] = []

    def run(self):
        ret = self.qapp.exec()
        self.close()
        return ret

    def close(self):
        self.session_bus.close()
        self.metadata_collector.close()
        self.thumbnailer.close()

    def close_controller(self, controller):
        self.controllers.remove(controller)
        controller.close()

    def show_files(self, files):
        controller = Controller(self)
        controller.set_files(files)
        controller.window.show()
        self.controllers.append(controller)

    def show_location(self, path):
        controller = Controller(self)
        controller.set_location(path)
        controller.window.show()
        self.controllers.append(controller)


# EOF #
