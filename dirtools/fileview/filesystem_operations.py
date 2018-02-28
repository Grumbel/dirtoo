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

    def _rename(self, oldpath: str, newpath: str) -> None:
        logger.info("FilesystemOperations._rename: \"%s\" to \"%s\"",
                    oldpath, newpath)
        os.rename(oldpath, newpath)

    def _show_rename_error_has_payload(self, location: Location, parent: Optional[QWidget]):
        msg = QMessageBox(parent)
        msg.setIcon(QMessageBox.Warning)
        msg.setWindowTitle("Rename Error")
        msg.setTextFormat(Qt.RichText)
        msg.setText("<b>Files inside an archive can't be renamed</b>")
        msg.setStandardButtons(QMessageBox.Ok)
        msg.exec()

    def _show_rename_error_os_error(self, location: Location, err: OSError, tb: str,
                                    parent: Optional[QWidget]):
        msg = QMessageBox(parent)
        msg.setIcon(QMessageBox.Critical)
        msg.setWindowTitle("Rename Error")
        msg.setTextFormat(Qt.RichText)
        msg.setText(
            "<b>Failed to rename \"<tt>{}</tt>\".</b>"
            .format(html.escape(location.get_basename())))
        msg.setInformativeText(
            "A failure occured while trying to rename the file.\n\n{}\n\n{}  →\n{}\n"
            .format(err.strerror, err.filename, err.filename2))  # type: ignore
        msg.setDetailedText(tb)
        msg.setStandardButtons(QMessageBox.Ok)
        msg.exec()

    def _show_rename_error_file_exists(self, location: Location, parent: Optional[QWidget]):
        msg = QMessageBox(parent)
        msg.setIcon(QMessageBox.Critical)
        msg.setWindowTitle("Rename Error")
        msg.setTextFormat(Qt.RichText)
        msg.setText("<b>Failed to rename \"{}\".</b>"
                    .format(html.escape(location.get_basename())))
        msg.setInformativeText("Can't rename file, filenname already exists.")
        msg.setStandardButtons(QMessageBox.Ok)
        msg.exec()

    def rename_location(self, location: Location, parent: Optional[QWidget]=None) -> None:
        if location.has_payload():
            self._show_rename_error_has_payload(location, parent)
        else:
            dialog = RenameDialog(parent)
            dialog.set_basename(location.get_basename())
            dialog.exec()

            if dialog.result() == QDialog.Accepted:
                oldpath = location.get_path()
                newpath = os.path.join(location.get_dirname(), dialog.get_new_basename())

                if oldpath == newpath:
                    logger.debug("FilesystemOperations.rename_location: source and destination are the same, "
                                 "skipping: \"%s\" to \"%s\"", oldpath, newpath)
                elif os.path.exists(newpath):
                    self._show_rename_error_file_exists(location, parent)
                else:
                    try:
                        self._rename(oldpath, newpath)
                    except OSError as err:
                        self._show_rename_error_os_error(location, err, traceback.format_exc(), parent)


# EOF #
