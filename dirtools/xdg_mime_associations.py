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


from typing import List, Set, Dict

# for parsing .desktop files
from xdg.DesktopEntry import DesktopEntry
import xdg.Mime

import xdg.BaseDirectory
from xdg.IniFile import IniFile
import os
import sys
import collections


def unique(lst):
    """Remove duplicate elements from a list."""
    return list(collections.OrderedDict.fromkeys(lst))


def generate_mimeapps_filenames():
    from xdg.BaseDirectory import (xdg_config_home, xdg_config_dirs,
                                   xdg_data_dirs)

    prefixes = os.environ.get('XDG_CURRENT_DESKTOP', None)
    prefixes = [""] + [p.lower() + "-" for p in prefixes.split(":") if p]

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
    results = filter(os.path.exists, results)

    return list(results)


def generate_mimeinfo_filenames():
    from xdg.BaseDirectory import (xdg_config_dirs, xdg_data_dirs)

    results = []

    for directory in xdg_data_dirs:
        results.append(os.path.join(directory, "applications", "mimeinfo.cache"))

    results = unique(results)
    results = filter(os.path.exists, results)

    return list(results)


def get_desktop_file(desktop_file):
    from xdg.BaseDirectory import xdg_data_dirs

    if not desktop_file:
        return None

    for directory in xdg_data_dirs:
        path = os.path.join(directory, "applications", desktop_file)
        if os.path.exists(path):
            return path

    return None


class XdgMimeAssociations:

    @staticmethod
    def system():
        """Load the systems mime database"""
        db = XdgMimeAssociations()
        db._read_mimeinfos()
        db._read_mimeapps()
        return db

    def __init__(self):
        self.default_mime2desktop: Dict[str, List[str]] = {}
        self.mime2desktop: Dict[str, Set[str]] = {}
        self.mimeinfos = []
        self.mimeapps = []

    def _read_mimeinfos(self):
        assert not self.mimeinfos
        self.mimeinfos = generate_mimeinfo_filenames()

        for filename in self.mimeinfos:
            ini = IniFile(filename)
            group = ini.content.get("MIME Cache")
            for mime, apps in group.items():
                for app in ini.getList(apps):
                    self.add_association(mime, app)

    def _read_mimeapps(self):
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

    def add_default_app(self, mime, app):
        self.add_association(mime, app)

        if mime not in self.default_mime2desktop:
            self.default_mime2desktop[mime] = []

        lst = self.default_mime2desktop[mime]
        # avoid duplicates while making sure the last addition is first
        if app in lst:
            lst.remove(app)
        lst.insert(0, app)

    def add_association(self, mime, app):
        if mime not in self.mime2desktop:
            self.mime2desktop[mime] = set()

        s = self.mime2desktop[mime]
        if app not in s:
            s.add(app)

    def remove_association(self, mime, app):
        s = self.mime2desktop.get(mime, None)
        if app and app in s:
            s.remove(app)

    def get_default_apps(self, mimetype):
        return self.default_mime2desktop.get(mimetype, [])

    def get_associations(self, mimetype):
        return list(self.mime2desktop.get(mimetype, []))


# EOF #
