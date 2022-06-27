# dirtoo - File and directory manipulation tools for Python
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


from typing import TYPE_CHECKING

from PyQt5.QtCore import QObject, pyqtSignal

from dirtoo.file_info import FileInfo

if TYPE_CHECKING:
    from dirtoo.fileview.application import FileViewApplication


class BookmarksProvider(QObject):

    sig_file_added = pyqtSignal(FileInfo)
    sig_finished = pyqtSignal()
    sig_message = pyqtSignal(str)

    def __init__(self, app: 'FileViewApplication'):
        super().__init__()

        self._app = app

    def close(self) -> None:
        pass

    def start(self) -> None:
        entries = self._app.bookmarks.get_entries()
        for location in entries:
            fileinfo = self._app.vfs.get_fileinfo(location)
            self.sig_file_added.emit(fileinfo)
        self.sig_finished.emit()


# EOF #
