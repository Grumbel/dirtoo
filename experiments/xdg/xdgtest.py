#!/usr/bin/env python3

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
import xdg.BaseDirectory
import xdg.Mime
from xdg.IniFile import IniFile
import os
import sys

# https://specifications.freedesktop.org/mime-apps-spec/mime-apps-spec-1.0.html

# Spec says to use 'XDG_CURRENT_DESKTOP', but that evaluates to "XFCE", 'XDG_MENU_PREFIX' evaluates to


def unique(lst):
    return list(dict.fromkeys(lst))


def generate_mimeapps_filenames():
    from xdg.BaseDirectory import (xdg_config_home, xdg_config_dirs,
                                   xdg_data_home, xdg_data_dirs)

    prefixes = os.environ.get('XDG_CURRENT_DESKTOP', None)
    prefixes = [""] + [p.lower() + "-" for p in prefixes.split(":") if p]

    results = []

    for prefix in prefixes:
        results.append(os.path.join(xdg_config_home, "{}mimeapps.list".format(prefix)))

    for directory in xdg_config_dirs:
        for prefix in prefixes:
            results.append(os.path.join(directory, "applications", "{}mimeapps.list".format(prefix)))

    for prefix in prefixes:
        results.append(os.path.join(xdg_data_home, "applications", "{}mimeapps.list".format(prefix)))

    for directory in xdg_data_dirs:
        for prefix in prefixes:
            results.append(os.path.join(directory, "applications", "{}mimeapps.list".format(prefix)))

    results = unique(results)
    results = filter(os.path.exists, results)

    return list(results)


def generate_mimeinfo_filenames():
    from xdg.BaseDirectory import (xdg_config_home, xdg_config_dirs,
                                   xdg_data_home, xdg_data_dirs)

    results = []

    for directory in [xdg_data_home] + xdg_data_dirs:
        results.append(os.path.join(directory, "applications", "mimeinfo.cache"))

    results = unique(results)
    results = filter(os.path.exists, results)

    return list(results)


class MimeDatabase:

    @staticmethod
    def system():
        """Load the systems mime database"""
        db = MimeDatabase()
        db._read_mimeinfos()
        db._read_mimeapps()
        return db

    def __init__(self):
        self.default_mime2desktop: Dict[str, List[str]] = {}
        self.mime2desktop: Dict[str, Set[str]] = {}

    def _read_mimeinfos(self):
        for filename in generate_mimeinfo_filenames():
            ini = IniFile(filename)
            group = ini.content.get("MIME Cache")
            for mime, apps in group.items():
                for app in ini.getList(apps):
                    self.add_association(mime, app)

    def _read_mimeapps(self):
        # Highest priority is first, so we reverse this
        for filename in reversed(generate_mimeapps_filenames()):
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
            for mime, apps in added.items():
                for app in reversed(ini.getList(apps)):
                    self.add_default_app(mime, app)

    def add_default_app(self, mime, app):
        self.add_association(mime, app)

        if mime not in self.default_mime2desktop:
            self.default_mime2desktop[mime] = []

        lst = self.default_mime2desktop[mime]
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


def main(argv):

    # xdg.Mime.get_type_by_name("/tmp/foo.png")
    # xdg.Mime.get_type_by_content("/tmp/foo.png")

    for p in generate_mimeapps_filenames():
        print(p)
    print("---------")
    for p in generate_mimeinfo_filenames():
        print(p)
    print("---------")

    mimedb = MimeDatabase.system()
    print(mimedb.get_default_apps("text/plain"))
    print(mimedb.get_default_apps("text/html"))
    print(mimedb.get_associations("text/html"))
    print(mimedb.get_default_apps("image/jpeg"))
    print(mimedb.get_default_apps("image/png"))

    # xdg.BaseDirectory.xdg_config_dirs

    # for datadir in xdg.BaseDirectory.xdg_data_dirs:
    #     prefixes = [x for x in [os.environ.get('XDG_MENU_PREFIX', None), ''] if x is not None]
    #     for prefix in prefixes:
    #         filename = os.path.join(datadir, "applications/{}defaults.list".format(prefix))
    #         if os.path.exists(filename):
    #             inifile = IniFile(filename)
    #             result = inifile.get("text/plain", group="Default Applications")
    #             if result:
    #                 print("{}:\n{!r}".format(filename, [x for x in result.split(";") if x]))

    #         filename = os.path.join(datadir, "applications/{}mimeinfo.cache".format(prefix))
    #         if os.path.exists(filename):
    #             inifile = IniFile(filename)
    #             result = inifile.get("text/plain", group="MIME Cache")
    #             if result:
    #                 print("{}:\n{!r}".format(filename, [x for x in result.split(";") if x]))


if __name__ == "__main__":
    main(sys.argv)


# EOF #

