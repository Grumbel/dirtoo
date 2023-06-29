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


from PyQt6.QtWidgets import (QDialog, QDialogButtonBox, QVBoxLayout,
                             QGroupBox, QCheckBox, QSpinBox, QLabel,
                             QLineEdit)

from dirtoo.fileview.settings import settings


class PreferencesDialog(QDialog):

    def __init__(self) -> None:
        super().__init__()

        self._applications_group_box: QGroupBox
        self._layout_group_box: QGroupBox
        self._cache_group_box: QGroupBox
        self._transfer_group_box: QGroupBox

        self.setWindowTitle("dirtoo Preferences")
        self._make_gui()

    def _make_gui(self) -> None:
        self._vbox = QVBoxLayout()

        self._vbox.addWidget(self._make_applications_box())
        self._vbox.addWidget(self._make_transfer_group_box())
        self._vbox.addWidget(self._make_layout_group_box())
        self._vbox.addWidget(self._make_cache_group_box())

        self._button_box = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        self._button_box.rejected.connect(self.reject)
        self._vbox.addWidget(self._button_box)

        self.setLayout(self._vbox)

    def _make_applications_box(self) -> QGroupBox:
        self._applications_group_box = QGroupBox("Applications")
        vbox = QVBoxLayout()

        checkbox = QCheckBox("Open Archives Internally")
        checkbox.setChecked(settings.value("globals/open_archives", True, bool))
        checkbox.stateChanged.connect(lambda state: settings.set_value("globals/open_archives", state))
        vbox.addWidget(checkbox)

        self._applications_group_box.setLayout(vbox)

        return self._applications_group_box

    def _make_layout_group_box(self) -> QGroupBox:
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
        return self._layout_group_box

    def _make_cache_group_box(self) -> QGroupBox:
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
        return self._cache_group_box

    def _make_transfer_group_box(self) -> QGroupBox:
        self._transfer_group_box = QGroupBox("Transfer")
        vbox = QVBoxLayout()

        checkbox = QCheckBox("Automatically close TransferDialog on completition")
        checkbox.setChecked(settings.value("globals/close_on_transfer_completed", True, bool))
        checkbox.stateChanged.connect(lambda state: settings.set_value("globals/close_on_transfer_completed", state))
        vbox.addWidget(checkbox)

        self._transfer_group_box.setLayout(vbox)
        return self._transfer_group_box


# EOF #
