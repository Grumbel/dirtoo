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


from typing import List

import argparse
import sys

from dirtoo.xdg_mime_associations import XdgMimeAssociations
from dirtoo.xdg_desktop import get_desktop_file


# https://specifications.freedesktop.org/mime-apps-spec/mime-apps-spec-1.0.html


def parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Query the systems mime associations")
    parser.add_argument("MIMETYPE", nargs='?')
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be verbose")
    return parser.parse_args(argv[1:])


def main(argv: List[str]) -> int:
    args = parse_args(argv)
    mimeasc = XdgMimeAssociations.system()

    if args.MIMETYPE is None:
        print("MIME Associations:")
        for filename in mimeasc.mimeapps:
            print("  {}".format(filename))
        print()

        print("MIME Cache:")
        for filename in mimeasc.mimeinfos:
            print("  {}".format(filename))
        print()

        if args.verbose:
            print("Associations:")
            for mime, apps in mimeasc.mime2desktop.items():
                print("  MimeType: {}".format(mime))
                defaultapps = mimeasc.get_default_apps(mime)
                for app in apps:
                    print("    {}{}".format(get_desktop_file(app) or "{} (file not found)".format(app),
                                            "  (default)" if app in defaultapps else ""))
                print()
    else:
        mimetype = args.MIMETYPE

        defaults = mimeasc.get_default_apps(mimetype)
        assocs = mimeasc.get_associations(mimetype)

        print("mime-type: {}".format(mimetype))
        print()

        print("default applications:")
        for desktop in defaults:
            print("  {}".format(get_desktop_file(desktop) or "{} (file not found)".format(desktop)))
        print()

        print("associated applications:")
        for desktop in assocs:
            print("  {}".format(get_desktop_file(desktop) or "{} (file not found)".format(desktop)))
        print()

    return 0


def main_entrypoint() -> None:
    exit(main(sys.argv))


# EOF #
