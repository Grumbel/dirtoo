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


from typing import Optional, Set, List, cast

from PyQt5.QtCore import QObject, Qt, QEvent
from PyQt5.QtWidgets import QMenu, QFileDialog, QTextEdit
from PyQt5.QtGui import QIcon, QCursor, QMouseEvent, QContextMenuEvent

from dirtools.fileview.menu import Menu
from dirtools.fileview.file_view_window import FileViewWindow
from dirtools.xdg_desktop import get_desktop_entry, get_desktop_file
from dirtools.fileview.location import Location, Payload
from dirtools.fileview.controller import Controller


class Gui(QObject):

    def __init__(self, controller: Controller) -> None:
        super().__init__()

        self._controller: Controller = controller
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
            return cast(str, filename)

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

    def on_context_menu(self, pos) -> None:
        menu = QMenu()

        menu.addAction(QIcon.fromTheme('folder-new'), "Create Directory")
        menu.addAction(QIcon.fromTheme('document-new'), "Create Text File")
        menu.addSeparator()
        menu.addAction(self._controller.actions.edit_paste)
        menu.addSeparator()

        if self.location is not None:
            menu.addAction(QIcon.fromTheme('utilities-terminal'), "Open Terminal Here",
                           lambda location=self.location: self._controller.app.executor.launch_terminal(location))
            menu.addSeparator()

        menu.addAction(self._controller.actions.edit_select_all)
        menu.addSeparator()
        menu.addAction(QIcon.fromTheme('document-properties'), "Properties...")

        menu.exec(pos)
        self.fake_mouse()

    def on_item_context_menu(self, ev, item) -> None:
        if item.isSelected():
            selected_items = self._window.thumb_view._scene.selectedItems()
        else:
            self._controller.clear_selection()
            item.setSelected(True)
            selected_items = [item]

        menu = Menu()

        if item.fileinfo.is_archive():
            def make_extract(archive_location):
                location = archive_location.copy()
                location._payloads.append(Payload("archive", ""))
                return location

            def left_func(location=item.fileinfo.location()):
                self.set_location(make_extract(location))

            def middle_func(location=item.fileinfo.location()):
                self.new_controller().set_location(make_extract(location))

            menu.addDoubleAction(
                QIcon.fromTheme("package-x-generic"),
                "Open Archive",
                left_func, middle_func)
            menu.addSeparator()

        elif item.fileinfo.isdir():

            def left_func(location=item.fileinfo.location()):
                self.set_location(location)

            def middle_func(location=item.fileinfo.location()):
                self.new_controller().set_location(location)

            menu.addDoubleAction(
                QIcon.fromTheme("folder"),
                "Open Folder",
                left_func, middle_func)
            menu.addSeparator()

        files: List[Location] = []
        mimetypes: Set[str] = set()
        for sit in selected_items:
            location = sit.fileinfo.location()
            files.append(location)
            mimetypes.add(self._controller.app.mime_database.get_mime_type(location).name())

        apps_default_sets: List[Set[str]] = []
        apps_other_sets: List[Set[str]] = []
        for mimetype in mimetypes:
            apps_default_sets.append(set(self._controller.app.mime_associations.get_default_apps(mimetype)))
            apps_other_sets.append(set(self._controller.app.mime_associations.get_associations(mimetype)))

        default_apps = set.intersection(*apps_default_sets)
        other_apps = set.intersection(*apps_other_sets)

        default_apps = {get_desktop_file(app) for app in default_apps}
        other_apps = {get_desktop_file(app) for app in other_apps}

        if None in default_apps:
            default_apps.remove(None)

        if None in other_apps:
            other_apps.remove(None)

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
            menu.addAction("No applications available").setEnabled(False)
        else:
            make_launcher_menu(menu, default_apps)

        if other_apps:
            open_with_menu = QMenu("Open with...")
            make_launcher_menu(open_with_menu, other_apps)
            menu.addMenu(open_with_menu)

        menu.addSeparator()

        actions_menu = QMenu("Actions")
        actions_menu.addAction("Stack Selection...")
        actions_menu.addAction("Tag Selection...")
        actions_menu.addSeparator()
        actions_menu.addAction("Compress...")
        actions_menu.addAction("New Folder With Selection...")
        menu.addMenu(actions_menu)
        menu.addSeparator()

        if len(selected_items) == 1 and next(iter(mimetypes)) == "inode/directory":
            menu.addAction(QIcon.fromTheme('utilities-terminal'), "Open Terminal Here",
                           lambda location=item.fileinfo.location():
                           self._controller.app.executor.launch_terminal(location))
            menu.addSeparator()

        menu.addSeparator()
        menu.addAction(self._controller.actions.edit_cut)
        menu.addAction(self._controller.actions.edit_copy)
        menu.addSeparator()
        menu.addAction(self._controller.actions.edit_delete)
        menu.addAction("Move To Trash")
        menu.addSeparator()

        rename = menu.addAction(
            QIcon.fromTheme('rename'), 'Rename',
            lambda location=item.fileinfo.location():
            self.show_rename_dialog(location))
        rename.setShortcut('F2')
        rename.setStatusTip('Rename the current file')
        menu.addAction(rename)

        menu.addSeparator()
        menu.addAction("Properties...")

        if ev.reason() == QContextMenuEvent.Keyboard:
            pos = self._window.thumb_view.mapToGlobal(
                self._window.thumb_view.mapFromScene(
                    item.pos() + item.boundingRect().center()))
            print(pos, item.boundingRect())
            menu.exec(pos)
        else:
            menu.exec(ev.screenPos())
        self.fake_mouse()


# EOF #
