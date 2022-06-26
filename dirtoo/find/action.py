# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2015-2017 Ingo Ruhnke <grumbel@gmail.com>
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


from typing import List, Tuple

import os
import shlex
import string
import subprocess
import sys

from dirtoo.find.context import Context
from dirtoo.find.util import replace_item


class Action:

    def __init__(self) -> None:
        pass

    def file(self, root: str, filename: str) -> None:
        pass

    def directory(self, root: str, filename: str) -> None:
        pass

    def finish(self) -> None:
        pass


class PrinterAction(Action):

    def __init__(self, fmt_str: str, finisher: bool = False) -> None:
        super().__init__()

        self.fmt_str = fmt_str
        self.finisher = finisher

        self.file_count = 0
        self.size_total = 0

        self.ctx = Context()
        self.global_vars = globals().copy()
        self.global_vars.update(self.ctx.get_hash())

    def file(self, root: str, filename: str) -> None:
        self.file_count += 1

        fullpath = os.path.join(root, filename)

        byte_size = os.lstat(fullpath).st_size

        self.size_total += byte_size

        self.ctx.current_file = fullpath

        local_vars = {
            '_': os.path.basename(filename),
            'p': fullpath,
            'ap': os.path.abspath(fullpath),
            'apq': shlex.quote(os.path.abspath(fullpath)),
            'pq': shlex.quote(fullpath),
            'q': shlex.quote(filename),
        }

        fmt = string.Formatter()
        for (literal_text, field_name, format_spec, _) in fmt.parse(self.fmt_str):
            if literal_text is not None:
                sys.stdout.write(literal_text)

            if field_name is not None:
                value = eval(field_name, self.global_vars, local_vars)  # pylint: disable=W0123
                sys.stdout.write(format(value, format_spec))

    def finish(self) -> None:
        if self.finisher:
            print("-" * 72)
            print("{:>12}  {} files in total".format(self.ctx.sizehr(self.size_total), self.file_count))


class MultiAction(Action):

    def __init__(self) -> None:
        super().__init__()

        self.actions: List[Action] = []

    def add(self, action: Action) -> None:
        self.actions.append(action)

    def file(self, root: str, filename: str) -> None:
        for action in self.actions:
            action.file(root, filename)

    def directory(self, root: str, filename: str) -> None:
        for action in self.actions:
            action.directory(root, filename)

    def finish(self) -> None:
        for action in self.actions:
            action.finish()


class ExecAction(Action):

    def __init__(self, exec_str: str):
        super().__init__()

        self.on_file_cmd = None
        self.on_multi_cmd = None
        self.all_files: List[str] = []

        cmd = shlex.split(exec_str)

        if "{}+" in cmd:
            self.on_multi_cmd = cmd
        elif "{}" in cmd:
            self.on_file_cmd = cmd
        else:
            pass  # FIXME

    def file(self, root: str, name: str) -> None:
        if self.on_file_cmd:
            cmd = replace_item(self.on_file_cmd, "{}", [os.path.join(root, name)])
            subprocess.call(cmd)

        if self.on_multi_cmd:
            self.all_files.append(os.path.join(root, name))

    def directory(self, root: str, name: str) -> None:
        pass

    def finish(self) -> None:
        if self.on_multi_cmd:
            multi_cmd = replace_item(self.on_multi_cmd, "{}+", self.all_files)
            subprocess.call(multi_cmd)


class ExprSorterAction(Action):

    def __init__(self, expr, reverse, find_action):
        super().__init__()
        self.expr = expr
        self.find_action = find_action
        self.reverse = reverse

        self.files: List[Tuple[str, str]] = []

        self.ctx = Context()
        self.global_vars = globals().copy()
        self.global_vars.update(self.ctx.get_hash())

    def file(self, root: str, filename: str) -> None:
        self.files.append((root, filename))

    def directory(self, root: str, filename: str) -> None:
        pass

    def finish(self) -> None:
        files3: List[Tuple[str, str, str]] = []
        if self.expr:
            for root, filename in self.files:
                fullpath = os.path.join(root, filename)
                self.ctx.current_file = fullpath
                local_vars = {
                    'p': fullpath,
                    '_': os.path.basename(filename)
                }
                key = eval(self.expr, self.global_vars, local_vars)  # pylint: disable=W0123

                files3.append((key, root, filename))

            files3 = sorted(files3, key=lambda x: x[0], reverse=self.reverse)
            files = [(b, c) for _, b, c in files3]
        else:
            if self.reverse:
                files = list(reversed(self.files))
            else:
                files = self.files

        for root, filename in files:
            self.find_action.file(root, filename)
        self.find_action.finish()


# EOF #
