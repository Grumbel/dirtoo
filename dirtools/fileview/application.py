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

import traceback
import os
import signal
import logging
import logging.config
import xdg.BaseDirectory

from PyQt5.QtGui import QPixmapCache
from PyQt5.QtWidgets import QApplication
from PyQt5.QtDBus import QDBusConnection

from dirtools.fileview.controller import Controller
from dirtools.fileview.thumbnailer import Thumbnailer
from dirtools.dbus_thumbnail_cache import DBusThumbnailCache
from dirtools.fileview.mime_database import MimeDatabase
from dirtools.fileview.metadata_collector import MetaDataCollector
from dirtools.fileview.settings import settings

logger = logging.getLogger(__name__)


class FileViewApplication:

    def __init__(self):
        # Allow Ctrl-C killing of the Qt app, see:
        # http://stackoverflow.com/questions/4938723/
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        self.config_dir = os.path.join(xdg.BaseDirectory.xdg_config_home, "dt-fileview")
        self.config_file = os.path.join(self.config_dir, "config.ini")
        if not os.path.isdir(self.config_dir):
            os.makedirs(self.config_dir)

        logging_config = os.path.join(self.config_dir, "logging.ini")
        if os.path.exists(logging_config):
            print("loading logger config from: {}".format(logging_config))
            try:
                logging.config.fileConfig(logging_config,
                                          defaults=None, disable_existing_loggers=False)
            except Exception as err:
                print(traceback.format_exc())

        settings.init(self.config_file)

        QPixmapCache.setCacheLimit(102400)

        self.qapp = QApplication([])
        self.qapp.setQuitOnLastWindowClosed(False)
        self.qapp.lastWindowClosed.connect(self.on_last_window_closed)

        self.thumbnailer = Thumbnailer()
        self.metadata_collector = MetaDataCollector()
        self.session_bus = QDBusConnection.sessionBus()
        self.dbus_thumbnail_cache = DBusThumbnailCache(self.session_bus)
        self.mime_database = MimeDatabase()

        self.controllers: List[Controller] = []

    def on_last_window_closed(self):
        self.quit()

    def quit(self):
        self.close()
        self.qapp.quit()

    def run(self):
        return self.qapp.exec()

    def close(self):
        settings.save()

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

    def show_filelist_stream(self, stream):
        controller = Controller(self)
        controller.set_filelist_stream(stream)
        controller.window.show()
        self.controllers.append(controller)

    def show_location(self, path):
        controller = Controller(self)
        controller.set_location(path)
        controller.window.show()
        self.controllers.append(controller)


# EOF #
