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

from dirtools.dbus_thumbnail_cache import DBusThumbnailCache
from dirtools.fileview.bookmarks import Bookmarks
from dirtools.fileview.executor import Executor
from dirtools.fileview.history import History
from dirtools.fileview.location import Location
from dirtools.fileview.metadata_collector import MetaDataCollector
from dirtools.fileview.mime_database import MimeDatabase
from dirtools.fileview.settings import settings
from dirtools.fileview.thumbnailer import Thumbnailer
from dirtools.fileview.virtual_filesystem import VirtualFilesystem
from dirtools.fileview.filelist_stream import FileListStream
from dirtools.xdg_mime_associations import XdgMimeAssociations

logger = logging.getLogger(__name__)


class FileViewApplication:

    def __init__(self) -> None:
        # Allow Ctrl-C killing of the Qt app, see:
        # http://stackoverflow.com/questions/4938723/
        signal.signal(signal.SIGINT, signal.SIG_DFL)

        self.cache_dir = os.path.join(xdg.BaseDirectory.xdg_cache_home, "dt-fileview")
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

        self.file_history = History(os.path.join(self.config_dir, "file.txt"))
        self.location_history = History(os.path.join(self.config_dir, "locations.txt"))
        self.bookmarks = Bookmarks(os.path.join(self.config_dir, "bookmarks.txt"))

        QPixmapCache.setCacheLimit(102400)

        self.qapp = QApplication([])
        self.qapp.setQuitOnLastWindowClosed(False)
        self.qapp.lastWindowClosed.connect(self.on_last_window_closed)

        self.vfs = VirtualFilesystem(self.cache_dir)
        self.executor = Executor(self)
        self.thumbnailer = Thumbnailer(self.vfs)
        self.metadata_collector = MetaDataCollector(self.vfs)
        self.session_bus = QDBusConnection.sessionBus()
        self.dbus_thumbnail_cache = DBusThumbnailCache(self.session_bus)
        self.mime_database = MimeDatabase(self.vfs)
        self.mime_associations = XdgMimeAssociations.system()

        self.controllers: List[Controller] = []

    def on_last_window_closed(self) -> None:
        self.quit()

    def quit(self) -> None:
        self.close()
        self.qapp.quit()

    def run(self):
        return self.qapp.exec()

    def close(self) -> None:
        settings.save()

        self.metadata_collector.close()
        self.thumbnailer.close()
        self.vfs.close()

    def close_controller(self, controller: 'Controller') -> None:
        self.controllers.remove(controller)
        controller.close()

    def new_controller(self) -> 'Controller':
        controller = Controller(self)
        controller.window.show()
        self.controllers.append(controller)
        return controller

    def show_files(self, files: List[Location]) -> None:
        controller = Controller(self)
        controller.set_files(files)
        controller.window.show()
        self.controllers.append(controller)

    def show_filelist_stream(self, stream: FileListStream) -> None:
        controller = Controller(self)
        controller.set_filelist_stream(stream)
        controller.window.show()
        self.controllers.append(controller)

    def show_location(self, location: Location) -> None:
        self.location_history.append(location)

        controller = Controller(self)
        controller.set_location(location)
        controller.window.show()
        self.controllers.append(controller)


from dirtools.fileview.controller import Controller  # noqa: E401, E402


# EOF #
