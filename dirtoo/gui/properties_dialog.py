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


from datetime import datetime
import grp
import pwd
import stat
import io

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (QWidget, QDialog, QPushButton, QLineEdit,
                             QGroupBox, QGridLayout, QVBoxLayout, QDialogButtonBox,
                             QLabel, QTabWidget, QPlainTextEdit)

import bytefmt

from dirtoo.file_info import FileInfo


class PropertiesDialog(QDialog):

    def __init__(self, fileinfo: FileInfo, parent: QWidget) -> None:
        super().__init__(parent)

        self._fileinfo: FileInfo = fileinfo

        self._make_gui()

    def _make_gui(self) -> None:
        self.setWindowTitle("Properties of {}".format("TestName"))

        # Widgets
        icon_label = QLabel()
        icon_label.setPixmap(QIcon.fromTheme("document").pixmap(48))
        icon_label.setAlignment(Qt.AlignHCenter)
        name_label = QLabel(self._fileinfo.basename())
        name_label.setAlignment(Qt.AlignHCenter)

        general_tab = self._make_general_tab()
        metadata_tab = self._make_metadata_tab()

        tab_widget = QTabWidget()
        tab_widget.addTab(general_tab, "General")
        tab_widget.addTab(metadata_tab, "MetaData")

        button_box = QDialogButtonBox(QDialogButtonBox.Close | QDialogButtonBox.Reset)
        button_box.rejected.connect(self.reject)

        # Layout
        vbox = QVBoxLayout()
        vbox.addWidget(icon_label)
        vbox.addWidget(name_label)
        vbox.addWidget(tab_widget)
        vbox.addWidget(button_box)

        self.setLayout(vbox)

    def _make_metadata_tab(self) -> QWidget:
        # Widgets
        textedit = QPlainTextEdit()
        # textedit.setLineWrapMode(QPlainTextEdit.NoWrap)

        out = io.StringIO()
        for key in self._fileinfo.get_metadata_keys():
            print("{}:\n{!r}\n".format(key, self._fileinfo.get_metadata(key)), file=out)
        textedit.setPlainText(out.getvalue())

        # Layout
        vbox = QVBoxLayout()
        vbox.addWidget(textedit)

        widget = QWidget()
        widget.setLayout(vbox)
        return widget

    def _make_general_tab(self) -> QWidget:
        # Widgets
        size_box = QGroupBox("Basic")

        name_label = QLabel("Name:")
        name_edit = QLineEdit(self._fileinfo.basename())

        abspath_label = QLabel("AbsPath:")
        abspath_edit = QLineEdit(self._fileinfo.abspath())

        size_label = QLabel("Size:")
        size_edit = QLineEdit(bytefmt.humanize(self._fileinfo.size()))
        size_edit.setReadOnly(True)

        mimetype_label = QLabel("MimeType:")
        mimetype_edit = QLineEdit(self._fileinfo.get_metadata_or("mime-type", "<unknown>"))
        mimetype_edit.setReadOnly(True)

        ownership_box = QGroupBox("Ownership")

        user_label = QLabel("User:")
        user_edit = QLineEdit("{} ({})".format(pwd.getpwuid(self._fileinfo.uid()).pw_name,
                                               self._fileinfo.uid()))
        user_edit.setReadOnly(True)

        group_label = QLabel("Group:")
        group_edit = QLineEdit("{} ({})".format(grp.getgrgid(self._fileinfo.gid()).gr_name,
                                                self._fileinfo.gid()))
        group_edit.setReadOnly(True)

        stat_result = self._fileinfo.stat()
        mode = stat_result.st_mode if stat_result is not None else 0

        access_box = QGroupBox("Access Control")
        access_user_label = QLabel("User:")

        access_user_read = QPushButton("Read")
        access_user_read.setCheckable(True)
        access_user_read.setChecked(bool(stat.S_IRUSR & mode))

        access_user_write = QPushButton("Write")
        access_user_write.setCheckable(True)
        access_user_write.setChecked(bool(stat.S_IWUSR & mode))

        access_user_exec = QPushButton("Execute")
        access_user_exec.setCheckable(True)
        access_user_exec.setChecked(bool(stat.S_IXUSR & mode))

        access_group_label = QLabel("Group:")

        access_group_read = QPushButton("Read")
        access_group_read.setCheckable(True)
        access_group_read.setChecked(bool(stat.S_IRGRP & mode))

        access_group_write = QPushButton("Write")
        access_group_write.setCheckable(True)
        access_group_write.setChecked(bool(stat.S_IWGRP & mode))

        access_group_exec = QPushButton("Execute")
        access_group_exec.setCheckable(True)
        access_group_exec.setChecked(bool(stat.S_IXGRP & mode))

        access_other_label = QLabel("Other:")

        access_other_read = QPushButton("Read")
        access_other_read.setCheckable(True)
        access_other_read.setChecked(bool(stat.S_IROTH & mode))

        access_other_write = QPushButton("Write")
        access_other_write.setCheckable(True)
        access_other_write.setChecked(bool(stat.S_IWOTH & mode))

        access_other_exec = QPushButton("Execute")
        access_other_exec.setCheckable(True)
        access_other_exec.setChecked(bool(stat.S_IXOTH & mode))

        access_special_label = QLabel("Special:")
        access_special_setuid = QPushButton("SetUid")
        access_special_setuid.setCheckable(True)
        access_special_setuid.setChecked(bool(stat.S_ISUID & mode))

        access_special_setgid = QPushButton("SetGid")
        access_special_setgid.setCheckable(True)
        access_special_setgid.setChecked(bool(stat.S_ISGID & mode))

        access_special_sticky = QPushButton("Sticky")
        access_special_sticky.setCheckable(True)
        access_special_sticky.setChecked(bool(stat.S_ISVTX & mode))

        def timestamp2str(time: float) -> str:
            dt = datetime.fromtimestamp(time)
            dtstr = dt.strftime("%Y-%m-%d %H:%M:%S")
            return dtstr

        time_box = QGroupBox("Time")

        atime_label = QLabel("Access:")
        atime_edit = QLineEdit(timestamp2str(self._fileinfo.atime()))
        atime_edit.setReadOnly(True)

        mtime_label = QLabel("Modify:")
        mtime_edit = QLineEdit(timestamp2str(self._fileinfo.mtime()))
        mtime_edit.setReadOnly(True)

        ctime_label = QLabel("Change:")
        ctime_edit = QLineEdit(timestamp2str(self._fileinfo.ctime()))
        ctime_edit.setReadOnly(True)

        # Layout
        vbox = QVBoxLayout()

        grid = QGridLayout()
        grid.addWidget(name_label, 0, 0, Qt.AlignRight)
        grid.addWidget(name_edit, 0, 1)
        grid.addWidget(abspath_label, 1, 0, Qt.AlignRight)
        grid.addWidget(abspath_edit, 1, 1)
        grid.addWidget(size_label, 2, 0, Qt.AlignRight)
        grid.addWidget(size_edit, 2, 1)
        grid.addWidget(mimetype_label, 3, 0, Qt.AlignRight)
        grid.addWidget(mimetype_edit, 3, 1)

        size_box.setLayout(grid)
        vbox.addWidget(size_box)

        grid = QGridLayout()
        grid.addWidget(user_label, 0, 0, Qt.AlignRight)
        grid.addWidget(user_edit, 0, 1)
        grid.addWidget(group_label, 1, 0, Qt.AlignRight)
        grid.addWidget(group_edit, 1, 1)
        ownership_box.setLayout(grid)
        vbox.addWidget(ownership_box)

        grid = QGridLayout()
        grid.addWidget(atime_label, 0, 0, Qt.AlignRight)
        grid.addWidget(atime_edit, 0, 1)
        grid.addWidget(mtime_label, 1, 0, Qt.AlignRight)
        grid.addWidget(mtime_edit, 1, 1)
        grid.addWidget(ctime_label, 2, 0, Qt.AlignRight)
        grid.addWidget(ctime_edit, 2, 1)
        time_box.setLayout(grid)
        vbox.addWidget(time_box)

        grid = QGridLayout()
        grid.addWidget(access_user_label, 0, 0, Qt.AlignRight)
        grid.addWidget(access_user_read, 0, 1)
        grid.addWidget(access_user_write, 0, 2)
        grid.addWidget(access_user_exec, 0, 3)

        grid.addWidget(access_group_label, 1, 0, Qt.AlignRight)
        grid.addWidget(access_group_read, 1, 1)
        grid.addWidget(access_group_write, 1, 2)
        grid.addWidget(access_group_exec, 1, 3)

        grid.addWidget(access_other_label, 2, 0, Qt.AlignRight)
        grid.addWidget(access_other_read, 2, 1)
        grid.addWidget(access_other_write, 2, 2)
        grid.addWidget(access_other_exec, 2, 3)

        grid.addWidget(access_special_label, 3, 0, Qt.AlignRight)
        grid.addWidget(access_special_setuid, 3, 1)
        grid.addWidget(access_special_setgid, 3, 2)
        grid.addWidget(access_special_sticky, 3, 3)
        access_box.setLayout(grid)
        vbox.addWidget(access_box)

        widget = QWidget()
        widget.setLayout(vbox)
        return widget


# EOF #
