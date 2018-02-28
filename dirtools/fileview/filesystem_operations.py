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

import html
import logging
import os
import traceback

from dirtools.fileview.location import Location
from dirtools.fileview.rename_dialog import RenameDialog

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QWidget, QDialog, QMessageBox

logger = logging.getLogger(__name__)


class FilesystemOperations:
    """Filesystem operations for the GUI. Messageboxes and dialogs will be
    shown on errors and conflicts."""

    def __init__(self, logfile=None):
        self._logfile = logfile

    def do_rename(self, oldpath: str, newpath: str) -> None:
        os.rename(oldpath, newpath)

    # def _show_message(parent: Optional[QWidget]=None):

    def rename_location(self, location: Location, parent: Optional[QWidget]=None) -> None:
        if location.has_payload():
            msg = QMessageBox(parent)
            msg.setIcon(QMessageBox.Warning)
            msg.setWindowTitle("Rename Error")
            msg.setText("Files inside an archive can't be renamed")
            msg.setStandardButtons(QMessageBox.Ok)
            msg.exec()
        else:
            dialog = RenameDialog(parent)
            dialog.set_basename(location.get_basename())
            dialog.exec()
            if dialog.result() == QDialog.Accepted:
                oldpath = location.get_path()
                newpath = os.path.join(location.get_dirname(), dialog.get_new_basename())
                if not os.path.exists(newpath):
                    logger.info("Controller.show_rename_dialog: renaming \"%s\" to \"%s\"",
                                oldpath, newpath)
                    try:
                        self.do_rename(oldpath, newpath)
                    except OSError as err:
                        msg = QMessageBox(parent)
                        msg.setIcon(QMessageBox.Critical)
                        msg.setWindowTitle("Rename Error")
                        msg.setTextFormat(Qt.RichText)
                        msg.setText(
                            "<b>Failed to rename \"<tt>{}</tt>\".</b>"
                            .format(html.escape(location.get_basename())))
                        msg.setInformativeText(
                            "A failure occured while trying to rename the file.\n\n{}\n\n{}  →\n{}"
                            .format(err.strerror, err.filename, err.filename2))  # type: ignore
                        msg.setDetailedText(traceback.format_exc())
                        msg.setStandardButtons(QMessageBox.Ok)
                        msg.exec()
                else:
                    msg = QMessageBox(parent)
                    msg.setIcon(QMessageBox.Critical)
                    msg.setWindowTitle("Rename Error")
                    msg.setText("Failed to rename \"{}\".".format(location.get_basename()))
                    msg.setInformativeText("Can't rename file, filenname already exists.")
                    msg.setStandardButtons(QMessageBox.Ok)
                    msg.exec()


# EOF #
