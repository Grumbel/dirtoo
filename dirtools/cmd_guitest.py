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


from typing import List, Dict, Callable

import logging
import signal
import sys
import argparse
import tempfile
import threading

from PyQt5.QtCore import QThread, QObject
from PyQt5.QtWidgets import QApplication, QDialog

from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.conflict_dialog import ConflictDialog
from dirtools.fileview.transfer_request_dialog import TransferRequestDialog
from dirtools.fileview.transfer_dialog import TransferDialog
from dirtools.fileview.rename_dialog import RenameDialog
from dirtools.fileview.properties_dialog import PropertiesDialog
from dirtools.fileview.about_dialog import AboutDialog
from dirtools.fileview.create_dialog import CreateDialog
from dirtools.fileview.preferences_dialog import PreferencesDialog
from dirtools.fileview.settings import settings

logger = logging.getLogger(__name__)


def parse_args(args: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Test GUI Widgets")
    parser.add_argument("DIALOG", nargs='?')
    parser.add_argument("-l", "--list", action='store_true', default=False,
                        help="List available dialogs")
    return parser.parse_args(args)


def make_transfer_request_dialog() -> TransferRequestDialog:
    dialog = TransferRequestDialog(
        [
            "/home/juser/test.txt",
            "/home/juser/README.md",
            "/home/juser/NotAFile.c",
            "/home/juser/NotAFile.c",
        ],
        "/home/juser/Target Directory",
        None)

    return dialog


class TransferDialogTest(QObject):

    def __init__(self, dialog: TransferDialog, parent: QObject) -> None:
        super().__init__()
        self._dialog = dialog

    def on_started(self) -> None:
        sleep_time = 1000

        self._dialog.sig_link.emit("/home/juser/symlink_file")
        self.thread().msleep(sleep_time)
        self._dialog.sig_move.emit("/home/juser/move_file")
        self.thread().msleep(sleep_time)

        for i in range(5):
            src = "/home/juser/foobar{}".format(i)
            self._dialog.sig_copy_begin.emit(src)
            total = 100000
            for j in range(100 + 1):
                self._dialog.sig_copy_progress.emit(src, j * total // 100, total)
                self.thread().msleep(sleep_time // 20)
            self._dialog.sig_copy_end.emit(src)
            self.thread().msleep(sleep_time)

        self._dialog.sig_move.emit("/home/juser/second_last_file")
        self.thread().msleep(sleep_time)
        self._dialog.sig_move.emit("/home/juser/last_file")
        self.thread().msleep(sleep_time)
        self._dialog.sig_transfer_complete.emit()


g_keep_alive = []


def make_transfer_dialog() -> TransferDialog:
    dialog = TransferDialog("/home/juser/Target Directory", None)

    thread = QThread(dialog)
    worker = TransferDialogTest(dialog, dialog)
    worker.moveToThread(thread)
    thread.started.connect(worker.on_started)
    thread.start()

    global g_keep_alive
    g_keep_alive += [worker, thread]

    return dialog


def main(argv: List[str]) -> None:
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    args = parse_args(argv[1:])
    app = QApplication([])  # noqa: F841

    dialog_spec = args.DIALOG

    tmpfile = tempfile.mkstemp()[1]
    settings.init(tmpfile)

    dialog_factory: Dict[str, Callable[[], QDialog]] = {
        'AboutDialog': lambda: AboutDialog(),
        'ConflictDialog': lambda: ConflictDialog(None),
        'CreateDialog-folder': lambda: CreateDialog(CreateDialog.FOLDER, None),
        'CreateDialog-file': lambda: CreateDialog(CreateDialog.TEXTFILE, None),
        'TransferRequestDialog': make_transfer_request_dialog,
        'TransferDialog': make_transfer_dialog,
        'PreferencesDialog': lambda: PreferencesDialog(),
        'PropertiesDialog': lambda: PropertiesDialog(FileInfo.from_path("/tmp/"), None),
        'RenameDialog': lambda: RenameDialog(None),
    }

    if args.list or dialog_spec is None:
        for k in dialog_factory.keys():
            print(k)
    else:
        dialog_func = dialog_factory[dialog_spec]
        dialog = dialog_func()
        dialog.exec()
        print("result:", dialog.result())


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
