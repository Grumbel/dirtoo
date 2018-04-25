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

from PyQt5.QtWidgets import QApplication, QDialog

from dirtools.fileview.file_info import FileInfo
from dirtools.fileview.conflict_dialog import ConflictDialog
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



def make_transfer_dialog() -> None:
    dialog = TransferDialog(
        [
            "/home/juser/test.txt",
            "/home/juser/README.md",
            "/home/juser/NotAFile.c",
            "/home/juser/NotAFile.c",
        ],
        "/home/juser/Target Directory",
        None)

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
        'TransferDialog': make_transfer_dialog,
        'PreferencesDialog': lambda: PreferencesDialog(),
        'PropertiesDialog': lambda: PropertiesDialog(FileInfo.from_path("/tmp/"), None),
        'RenameDialog': lambda: RenameDialog(None),
    }

    if args.list:
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
