# dirtool.py - diff tool for directories
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


from pkg_resources import resource_filename

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPixmap
from PyQt5.QtWidgets import QDialog, QVBoxLayout, QLabel, QDialogButtonBox, QSizePolicy


class AboutDialog(QDialog):

    def __init__(self) -> None:
        super().__init__()

        self.vbox = QVBoxLayout()
        self.vbox.setAlignment(Qt.AlignHCenter)
        label = QLabel()
        p = label.sizePolicy()
        p.setHorizontalPolicy(QSizePolicy.Expanding)
        p.setVerticalPolicy(QSizePolicy.Minimum)
        label.setSizePolicy(p)
        label.setPixmap(QPixmap(resource_filename("dirtools", "fileview/dt-fileview.svg")))
        self.vbox.addWidget(label)

        self.vbox = QVBoxLayout()

        label = QLabel()
        label.setPixmap(QPixmap(resource_filename("dirtools", "fileview/dt-fileview.svg")))
        self.vbox.addWidget(label)

        label = QLabel("dt-fileview")
        self.vbox.addWidget(label)

        label = QLabel("A file viewer and manager application.")
        self.vbox.addWidget(label)

        label = QLabel("Copyright (C) 2018\nIngo Ruhnke <grumbel@gmail.com>")
        self.vbox.addWidget(label)

        label = QLabel("Licensed under the GPLv3+")
        self.vbox.addWidget(label)

        box = QDialogButtonBox(QDialogButtonBox.Close)
        box.rejected.connect(self.reject)
        self.vbox.addWidget(box)

        self.setLayout(self.vbox)

    def reject(self):
        self.done(0)


# EOF #
