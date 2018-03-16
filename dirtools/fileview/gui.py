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


from typing import Optional

from PyQt5.QtCore import QObject, Qt, QEvent
from PyQt5.QtWidgets import QFileDialog, QTextEdit
from PyQt5.QtGui import QCursor, QMouseEvent

from dirtools.fileview.file_view_window import FileViewWindow


class Gui:

    def __init__(self, controller):
        self._controller = controller
        self._window = FileViewWindow(self._controller)
        self._filter_help = QTextEdit()

    def get_savefilename(self) -> Optional[str]:
        options = QFileDialog.Options()
        # options |= QFileDialog.DontUseNativeDialog
        filename, kind = QFileDialog.getSaveFileName(
            self._window,
            "QFileDialog.getSaveFileName()",
            "",  # dir
            "URL List (*.urls);;Path List (*.txt);;NUL Path List (*.nlst)",
            options=options)

        if filename == "":
            return None
        else:
            return filename

    def show_help(self, text: str) -> None:
        self._filter_help.setText(text)
        self._filter_help.resize(480, 800)
        self._filter_help.show()

    def fake_mouse(self) -> None:
        """Generate a fake mouse move event to force the ThumbView to update
        the hover item after a menu was displayed."""

        ev = QMouseEvent(QEvent.MouseMove,
                         self._window.mapFromGlobal(QCursor.pos()),
                         Qt.NoButton,
                         Qt.NoButton,
                         Qt.NoModifier)
        self._window.thumb_view.mouseMoveEvent(ev)


# EOF #
