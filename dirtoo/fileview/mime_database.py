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


from PyQt5.QtCore import QMimeType, QMimeDatabase
from PyQt5.QtGui import QIcon

from dirtoo.location import Location
from dirtoo.fileview.virtual_filesystem import VirtualFilesystem


class MimeDatabase:

    def __init__(self, vfs: VirtualFilesystem) -> None:
        self.vfs = vfs
        self.mime_db = QMimeDatabase()

    def get_mime_type(self, location: Location) -> QMimeType:
        return self.mime_db.mimeTypeForFile(self.vfs.get_stdio_name(location))

    def get_icon_from_mime_type(self, mimetype: QMimeType) -> QIcon:
        icon_name = mimetype.iconName()
        icon = QIcon.fromTheme(icon_name)
        if not icon.isNull():
            return icon

        for alias in mimetype.aliases():
            icon_name = alias.replace("/", "-", 1)
            icon = QIcon.fromTheme(icon_name)
            if not icon.isNull():
                return icon

        icon_name = mimetype.genericIconName()
        icon = QIcon.fromTheme(icon_name)
        if not icon.isNull():
            return icon

        return QIcon.fromTheme("application-octet-stream")


# EOF #
