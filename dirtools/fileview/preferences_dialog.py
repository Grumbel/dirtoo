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


from PyQt5.QtWidgets import (QDialog, QDialogButtonBox, QVBoxLayout,
                             QGroupBox, QCheckBox, QSpinBox, QLabel,
                             QLineEdit)


class PreferencesDialog(QDialog):

    def __init__(self):
        super().__init__()

        self.setWindowTitle("dt-fileview Preferences")
        self._make_gui()

    def _make_gui(self):
        self._vbox = QVBoxLayout()

        # Layout Box
        self._layout_group_box = QGroupBox("Layout")
        vbox = QVBoxLayout()
        checkbox = QCheckBox("Center Icons")
        vbox.addWidget(checkbox)

        label = QLabel("Horizontal Spacing")
        spinbox = QSpinBox()
        vbox.addWidget(label)
        vbox.addWidget(spinbox)

        label = QLabel("Vertical Spacing")
        spinbox = QSpinBox()
        vbox.addWidget(label)
        vbox.addWidget(spinbox)

        self._layout_group_box.setLayout(vbox)

        self._cache_group_box = QGroupBox("Cache")
        vbox = QVBoxLayout()
        label = QLabel("Maximum Cache Size")
        spinbox = QSpinBox()
        vbox.addWidget(label)
        vbox.addWidget(spinbox)

        label = QLabel("Extraction Cache Directory")
        lineedit = QLineEdit()
        vbox.addWidget(label)
        vbox.addWidget(lineedit)

        self._cache_group_box.setLayout(vbox)

        self._button_box = QDialogButtonBox(QDialogButtonBox.Close)
        self._button_box.rejected.connect(self.reject)

        self._vbox.addWidget(self._layout_group_box)
        self._vbox.addWidget(self._cache_group_box)
        self._vbox.addWidget(self._button_box)

        self.setLayout(self._vbox)


# EOF #
