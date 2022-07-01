# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018-2022 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import Optional

from PyQt5.QtCore import QObject, QSocketNotifier, pyqtSignal
from inotify_simple import INotify, flags as inotify_flags
import inotify_simple


DEFAULT_FLAGS = (
    inotify_flags.CREATE |
    inotify_flags.DELETE |
    inotify_flags.DELETE_SELF |
    inotify_flags.MOVE_SELF |
    inotify_flags.MODIFY |
    inotify_flags.ATTRIB |
    inotify_flags.MOVED_FROM |
    inotify_flags.MOVED_TO |
    inotify_flags.CLOSE_WRITE
)


class INotifyQt(QObject):

    sig_event = pyqtSignal(inotify_simple.Event)

    def __init__(self, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)

        self.inotify = INotify()
        self.qnotifier = QSocketNotifier(self.inotify.fd, QSocketNotifier.Read)
        self.qnotifier.activated.connect(self._on_activated)
        self.wd = None

    def add_watch(self, path: str, flags: inotify_flags = DEFAULT_FLAGS) -> None:
        self.wd = self.inotify.add_watch(path, flags)

    def _on_activated(self, fd: int) -> None:
        assert fd == self.inotify.fd

        for ev in self.inotify.read():
            self.sig_event.emit(ev)

    def close(self) -> None:
        del self.qnotifier
        self.inotify.close()


# EOF #
