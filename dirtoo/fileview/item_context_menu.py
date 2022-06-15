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


from typing import TYPE_CHECKING, cast, List, Set, Tuple, Optional

from PyQt5.QtWidgets import QMenu
from PyQt5.QtGui import QIcon

from dirtoo.location import Location, Payload
from dirtoo.fileview.menu import Menu
from dirtoo.xdg_desktop import get_desktop_entry, get_desktop_file

if TYPE_CHECKING:
    from dirtoo.fileview.controller import Controller  # noqa: F401


class ItemContextMenu(Menu):

    def __init__(self, controller: 'Controller', fileinfos: List) -> None:
        super().__init__()

        self._controller = controller
        self._fileinfos = fileinfos
        self._fileinfo = self._fileinfos[0]
        self._build_menu()

    def _build_menu(self):
        if self._fileinfo.is_archive():
            self._build_archive_menu()
        elif self._fileinfo.isdir():
            self._build_directory_menu

        self._build_open_menu()
        self._build_open_containing_folder()
        self._build_open_terminal_menu()
        self._build_edit_menu()
        self._build_actions_menu()
        self._build_rename_menu()
        self._build_properties_entry()

    def _build_archive_menu(self):
        def make_extract(archive_location):
            location = archive_location.copy()
            location._payloads.append(Payload("archive", ""))
            return location

        def left_func(location=self._fileinfo.location()):
            self._controller.set_location(make_extract(location))

        def middle_func(location=self._fileinfo.location()):
            self._controller.new_controller().set_location(make_extract(location))

        self.addDoubleAction(
            QIcon.fromTheme("package-x-generic"),
            "Open Archive",
            left_func, middle_func)
        self.addSeparator()

    def _build_directory_menu(self):
        def left_func(location=self._fileinfo.location()):
            self._controller.set_location(location)

        def middle_func(location=self._fileinfo.location()):
            self._controller.new_controller().set_location(location)

        self.addDoubleAction(
            QIcon.fromTheme("folder"),
            "Open Folder",
            left_func, middle_func)
        self.addSeparator()

    def _build_open_containing_folder(self):
        def left_func(location=self._fileinfo.location().parent()):
            self._controller.set_location(location)

        def middle_func(location=self._fileinfo.location().parent()):
            self._controller.new_controller().set_location(location)

        self.addDoubleAction(
            QIcon.fromTheme("folder"),
            "Open Containing Folder",
            left_func, middle_func)
        self.addSeparator()

    def _get_supported_apps(self, files: List[Location]) -> Tuple[Set[str], Set[str]]:
        mimetypes: Set[str] = set(
            self._controller.app.mime_database.get_mime_type(location).name()
            for location in files
        )

        apps_default_sets: List[Set[Optional[str]]] = [
            set(self._controller.app.mime_associations.get_default_apps(mimetype))
            for mimetype in mimetypes
        ]

        apps_other_sets: List[Set[Optional[str]]] = [
            set(self._controller.app.mime_associations.get_associations(mimetype))
            for mimetype in mimetypes
        ]

        default_apps = set.intersection(*apps_default_sets)
        other_apps = set.intersection(*apps_other_sets)

        default_apps = {get_desktop_file(app) for app in default_apps}
        other_apps = {get_desktop_file(app) for app in other_apps}

        if None in default_apps:
            default_apps.remove(None)

        if None in other_apps:
            other_apps.remove(None)

        return cast(Tuple[Set[str], Set[str]], (default_apps, other_apps))

    def _build_open_menu(self):
        files: List[Location] = [fi.location() for fi in self._fileinfos]

        default_apps, other_apps = self._get_supported_apps(files)

        def make_launcher_menu(menu, apps):
            entries = [get_desktop_entry(app) for app in apps]
            entries = sorted(entries, key=lambda x: x.getName())
            for entry in entries:
                action = menu.addAction(QIcon.fromTheme(entry.getIcon()), "Open With {}".format(entry.getName()))

                def on_action(checked, exe=entry.getExec(), files=files):
                    self._controller.app.file_history.append_group(files)
                    self._controller.app.executor.launch_multi_from_exec(exe, files)

                action.triggered.connect(on_action)

        if not default_apps:
            self.addAction("No applications available").setEnabled(False)
        else:
            make_launcher_menu(self, default_apps)

        if other_apps:
            open_with_menu = QMenu("Open with...", self)
            make_launcher_menu(open_with_menu, other_apps)
            self.addMenu(open_with_menu)

        self.addSeparator()

    def _build_actions_menu(self):
        actions_menu = QMenu("Actions", self)
        actions_menu.addAction(self._controller.actions.reload_thumbnails)
        actions_menu.addAction(self._controller.actions.reload_metadata)
        actions_menu.addAction(self._controller.actions.make_directory_thumbnails)

        actions_menu.addAction("Stack Selection...")
        actions_menu.addAction("Tag Selection...")
        actions_menu.addSeparator()
        actions_menu.addAction("Compress...")
        actions_menu.addAction("New Folder With Selection...")
        self.addMenu(actions_menu)
        self.addSeparator()

    def _build_open_terminal_menu(self):
        if len(self._fileinfos) > 1:
            return

        mimetype = self._controller.app.mime_database.get_mime_type(self._fileinfo.location()).name()
        if mimetype == "inode/directory":
            self.addAction(QIcon.fromTheme('utilities-terminal'), "Open Terminal Here",
                           lambda location=self._fileinfo.location():
                           self._controller.app.executor.launch_terminal(location))
            self.addSeparator()

    def _build_edit_menu(self):
        self.addAction(self._controller.actions.edit_cut)
        self.addAction(self._controller.actions.edit_copy)

        act = self.addAction(QIcon.fromTheme('edit-paste'), "Paste Into Folder")
        if self._fileinfo.isdir():
            act.triggered.connect(
                lambda: self._controller.on_edit_paste_into(self._fileinfo.location()))
            act.setEnabled(True)
        else:
            act.setEnabled(False)

        self.addSeparator()
        self.addAction(self._controller.actions.edit_delete)
        self.addAction("Move To Trash")
        self.addSeparator()

    def _build_rename_menu(self):
        rename = self.addAction(
            QIcon.fromTheme('rename'), 'Rename',
            lambda location=self._fileinfo.location():
            self._controller.show_rename_dialog(location))
        rename.setShortcut('F2')
        rename.setStatusTip('Rename the current file')
        self.addAction(rename)
        self.addSeparator()

    def _build_properties_entry(self):
        self.addAction("Properties...", lambda: self._controller.show_properties_dialog(self._fileinfo))


# EOF #
