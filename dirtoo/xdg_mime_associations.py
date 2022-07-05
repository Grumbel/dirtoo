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


from typing import Any, Sequence, Set, Dict

import os
import collections

from xdg.IniFile import IniFile


def unique(lst: Sequence[Any]) -> list[Any]:
    """Remove duplicate elements from a list."""
    return list(collections.OrderedDict.fromkeys(lst))


def generate_mimeapps_filenames() -> Sequence[str]:
    from xdg.BaseDirectory import (xdg_config_home, xdg_config_dirs,
                                   xdg_data_dirs)

    current_desktop = os.environ.get('XDG_CURRENT_DESKTOP', "")
    prefixes = [""] + [p.lower() + "-" for p in current_desktop.split(":") if p]

    results = []

    for prefix in prefixes:
        results.append(os.path.join(xdg_config_home, "{}mimeapps.list".format(prefix)))

    # 'defaults.list' is deprecated, but still in active use as of Ubuntu 17.10
    for directory in xdg_config_dirs:
        for prefix in prefixes:
            results.append(os.path.join(directory, "applications", "{}defaults.list".format(prefix)))
            results.append(os.path.join(directory, "applications", "{}mimeapps.list".format(prefix)))

    for directory in xdg_data_dirs:
        for prefix in prefixes:
            results.append(os.path.join(directory, "applications", "{}defaults.list".format(prefix)))
            results.append(os.path.join(directory, "applications", "{}mimeapps.list".format(prefix)))

    results = unique(results)
    results = list(filter(os.path.exists, results))

    return results


def generate_mimeinfo_filenames() -> Sequence[str]:
    from xdg.BaseDirectory import xdg_data_dirs

    results = []

    for directory in xdg_data_dirs:
        results.append(os.path.join(directory, "applications", "mimeinfo.cache"))

    results = unique(results)
    results = list(filter(os.path.exists, results))

    return results


class XdgMimeAssociations:

    @staticmethod
    def system() -> 'XdgMimeAssociations':
        """Load the systems mime database"""
        db = XdgMimeAssociations()
        db._read_mimeinfos()
        db._read_mimeapps()
        return db

    def __init__(self) -> None:
        self.default_mime2desktop: Dict[str, list[str]] = {}
        self.mime2desktop: Dict[str, Set[str]] = {}
        self.mimeinfos: Sequence[str] = []
        self.mimeapps: Sequence[str] = []

    def _read_mimeinfos(self) -> None:
        assert not self.mimeinfos
        self.mimeinfos = generate_mimeinfo_filenames()

        for filename in self.mimeinfos:
            ini = IniFile(filename)
            group = ini.content.get("MIME Cache")
            for mime, apps in group.items():
                for app in ini.getList(apps):
                    self.add_association(mime, app)

    def _read_mimeapps(self) -> None:
        assert not self.mimeapps
        self.mimeapps = generate_mimeapps_filenames()

        # Highest priority is first, so we reverse this
        for filename in reversed(self.mimeapps):
            ini = IniFile(filename)

            removed = ini.content.get("Removed Associations", {})
            for mime, apps in removed.items():
                for app in ini.getList(apps):
                    self.remove_association(mime, app)

            added = ini.content.get("Added Associations", {})
            for mime, apps in added.items():
                for app in ini.getList(apps):
                    self.add_association(mime, app)

            default = ini.content.get("Default Applications", {})
            for mime, apps in default.items():
                for app in reversed(ini.getList(apps)):
                    self.add_default_app(mime, app)

    def add_default_app(self, mime: str, app: str) -> None:
        self.add_association(mime, app)

        if mime not in self.default_mime2desktop:
            self.default_mime2desktop[mime] = []

        lst = self.default_mime2desktop[mime]
        # avoid duplicates while making sure the last addition is first
        if app in lst:
            lst.remove(app)
        lst.insert(0, app)

    def add_association(self, mime: str, app: str) -> None:
        if mime not in self.mime2desktop:
            self.mime2desktop[mime] = set()

        s = self.mime2desktop[mime]
        if app not in s:
            s.add(app)

    def remove_association(self, mime: str, app: str) -> None:
        s = self.mime2desktop.get(mime, None)
        if s and app in s:
            s.remove(app)

    def get_default_apps(self, mimetype: str) -> Sequence[str]:
        return self.default_mime2desktop.get(mimetype, [])

    def get_associations(self, mimetype: str) -> Sequence[str]:
        return list(self.mime2desktop.get(mimetype, []))


# EOF #
