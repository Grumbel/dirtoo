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


from typing import cast, Sequence, Tuple

from PyQt6.QtCore import Qt, QUrl, QByteArray


def parse_gnome_copied_files(data: bytes) -> Tuple[Qt.DropAction, Sequence[QUrl]]:
    lines = data.split(b'\n')

    if lines[0] == b"copy":
        action = Qt.DropAction.CopyAction
    elif lines[0] == b"cut":
        action = Qt.DropAction.MoveAction
    else:
        raise RuntimeError("unknown action: {!r}".format(lines[0]))

    urls = [QUrl.fromEncoded(cast(QByteArray, d)) for d in lines[1:]]

    return action, urls


def make_gnome_copied_files(action: Qt.DropAction, urls: Sequence[QUrl]) -> bytes:
    if action == Qt.DropAction.CopyAction:
        gnome_action = b'copy'
    elif action == Qt.DropAction.MoveAction:
        gnome_action = b'move'
    else:
        raise RuntimeError("unknown action: {}".format(action))

    result = (gnome_action + b'\n' +
              b'\n'.join([url.toString().encode()
                          for url in urls]))
    return result


# EOF #
