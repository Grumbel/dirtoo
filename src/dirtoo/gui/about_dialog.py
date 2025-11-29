# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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


from importlib.resources import files

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QPixmap
from PyQt6.QtWidgets import QDialog, QVBoxLayout, QLabel, QDialogButtonBox, QSizePolicy


class AboutDialog(QDialog):

    def __init__(self) -> None:
        super().__init__()

        self._build_gui()

    def _build_gui(self) -> None:
        # Widgets
        icon_label = QLabel()
        icon_label.setAlignment(Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignBottom)
        icon_label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        icon_label.setPixmap(QPixmap(str(files("dirtoo") / "icons/dirtoo.png")))

        text_label = QLabel(
            """
            <center>
            <h1>dirtoo</h1>
            <p>A file viewer and manager application.</p>
            <p><a href="https://github.com/Grumbel/dirtoo">https://github.com/Grumbel/dirtoo</a></p>
            <p>Copyright (C) 2018\nIngo Ruhnke &lt;<a href="mail:grumbel@gmail.com">grumbel@gmail.com</a>&gt;</p>
            <p>Licensed under the GPLv3+</p>
            </center>

            """
        )
        text_label.setTextFormat(Qt.TextFormat.RichText)
        text_label.setAlignment(Qt.AlignmentFlag.AlignHCenter | Qt.AlignmentFlag.AlignTop)
        text_label.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Expanding)
        text_label.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)

        button_box = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        button_box.rejected.connect(self.reject)

        # Layout
        vbox = QVBoxLayout()
        vbox.addStretch()
        vbox.addWidget(icon_label)
        vbox.addWidget(text_label)
        vbox.addStretch()
        vbox.addWidget(button_box)

        self.setLayout(vbox)


# EOF #
