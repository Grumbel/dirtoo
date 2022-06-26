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


from typing import Dict
from abc import ABC, abstractmethod

import os

from dirtoo.find.context import Context
from dirtoo.fileview.filter_expr_parser import FilterExprParser
from dirtoo.fileview.lazy_file_info import LazyFileInfo


class Filter(ABC):

    @abstractmethod
    def match_file(self, root: str, filename: str) -> bool:
        pass


class NoFilter(Filter):

    def __init__(self) -> None:
        pass

    def match_file(self, root: str, filename: str) -> bool:
        return True


class ExprFilter(Filter):

    def __init__(self, expr):
        self.expr = expr
        self.local_vars: Dict[str, str] = {}
        self.ctx = Context()
        self.global_vars = globals().copy()
        self.global_vars.update(self.ctx.get_hash())

    def match_file(self, root: str, filename: str) -> bool:
        fullpath = os.path.join(root, filename)

        self.ctx.current_file = fullpath
        local_vars = {
            'p': fullpath,
            '_': filename
        }
        result = eval(self.expr, self.global_vars, local_vars)  # pylint: disable=W0123
        return result


class SimpleFilter(Filter):

    @staticmethod
    def from_string(text: str) -> 'SimpleFilter':
        parser = FilterExprParser()
        filter_expr = parser.parse(text)
        return SimpleFilter(filter_expr)

    def __init__(self, expr) -> None:
        self._expr = expr

    def match_file(self, root: str, filename: str) -> bool:
        path = os.path.join(root, filename)

        fileinfo = LazyFileInfo.from_path(path)
        return self._expr(fileinfo)


# EOF #
