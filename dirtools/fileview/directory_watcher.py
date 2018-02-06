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


import logging
import os

from PyQt5.QtCore import QObject, QSocketNotifier, QThread, pyqtSignal

from inotify_simple import INotify, flags as inotify_flags, masks as inotify_masks
import inotify_simple

from dirtools.fileview.file_info import FileInfo


class INotifyQt(QObject):

    sig_event = pyqtSignal(inotify_simple.Event)

    def __init__(self, parent=None):
        super().__init__(parent)

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


class DirectoryWatcherWorker(QObject):

    sig_file_added = pyqtSignal(FileInfo)
    sig_file_removed = pyqtSignal(str)
    sig_file_changed = pyqtSignal(FileInfo)
    sig_error = pyqtSignal()
    sig_scandir_finished = pyqtSignal(list)

    sig_close_requested = pyqtSignal()
    sig_finished = pyqtSignal()

    def __init__(self, path):
        super().__init__()

        self.path = path
        self._close = False

        self.inotify = INotifyQt(self)
        self.inotify.add_watch(path)
        self.inotify.sig_event.connect(self.on_inotify_event)

        self.sig_close_requested.connect(self.on_close_requested)

    def close(self):
        self.inotify.close()
        del self.inotify

    def on_close_requested(self):
        self.close()
        self.sig_finished.emit()

    def process(self):
        fileinfos = []

        logging.debug("DirectoryWatcher.process: gather directory content")
        for entry in os.scandir(self.path):
            fileinfo = FileInfo.from_direntry(entry)
            fileinfos.append(fileinfo)

            if self._close:
                return

        self.sig_scandir_finished.emit(fileinfos)

    def on_inotify_event(self, ev):
        try:
            if ev.mask & inotify_flags.CREATE:
                self.sig_file_added.emit(FileInfo.from_filename(os.path.join(self.path, ev.name)))
            elif ev.mask & inotify_flags.DELETE:
                self.sig_file_removed.emit(os.path.join(self.path, ev.name))
            elif ev.mask & inotify_flags.DELETE_SELF:
                pass  # directory itself has disappeared
            elif ev.mask & inotify_flags.MOVE_SELF:
                pass  # directory itself has moved
            elif ev.mask & inotify_flags.MODIFY or ev.mask & inotify_flags.ATTRIB:
                self.sig_file_changed.emit(FileInfo.from_filename(os.path.join(self.path, ev.name)))
            elif ev.mask & inotify_flags.MOVED_FROM:
                self.sig_file_removed.emit(os.path.join(self.path, ev.name))
            elif ev.mask & inotify_flags.MOVED_TO:
                self.sig_file_added.emit(FileInfo(os.path.join(self.path, ev.name)))
            else:
                pass  # unhandled event
        except Exception as err:
            print("DirectoryWatcher:", err)


class DirectoryWatcher(QObject):

    def __init__(self, path):
        super().__init__()

        self.worker = DirectoryWatcherWorker(path)
        self.thread = QThread(self)
        self.worker.moveToThread(self.thread)

        self.thread.started.connect(self.worker.process)
        self.worker.sig_finished.connect(self.thread.quit)

    def start(self):
        self.thread.start()

    def close(self):
        self.worker._close = True
        self.worker.sig_close_requested.emit()
        self.thread.wait()

    @property
    def sig_file_added(self):
        return self.worker.sig_file_added

    @property
    def sig_file_removed(self):
        return self.worker.sig_file_removed

    @property
    def sig_file_changed(self):
        return self.worker.sig_file_changed

    @property
    def sig_scandir_finished(self):
        return self.worker.sig_scandir_finished


# EOF #
