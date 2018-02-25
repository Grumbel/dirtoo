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


import argparse
import os
import signal
import sys
import xdg.BaseDirectory

from PyQt5.QtCore import QCoreApplication

from dirtools.fileview.metadata_collector import MetaDataCollector
from dirtools.fileview.virtual_filesystem import VirtualFilesystem
from dirtools.fileview.location import Location


def parse_args(args):
    parser = argparse.ArgumentParser(description="Generate and show file metadata")
    parser.add_argument("FILE", nargs='+')
    parser.add_argument('-v', '--verbose', action='store_true', default=False,
                        help="Be more verbose")
    parser.add_argument('-r', '--recursive', action='store_true', default=False,
                        help="Recurse into directories")
    parser.add_argument('-d', '--delete', action='store_true', default=False,
                        help="Delete metadata for the given files")
    return parser.parse_args(args)


def main(argv):
    args = parse_args(argv[1:])

    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QCoreApplication([])

    cache_dir = os.path.join(xdg.BaseDirectory.xdg_cache_home, "dt-fileview")
    vfs = VirtualFilesystem(cache_dir)
    metadata_collector = MetaDataCollector(vfs)

    num_requests = 0

    def on_metadata_ready(filename, metadata):
        nonlocal num_requests, app
        print(filename)
        for k, v in metadata.items():
            print("    {}: {}".format(k, v))
        print()
        num_requests -= 1
        if num_requests == 0:
            app.quit()

    metadata_collector.sig_metadata_ready.connect(on_metadata_ready)

    def request(filename):
        nonlocal num_requests
        metadata_collector.request_metadata(Location.from_path(filename))
        num_requests += 1

    for filename in args.FILE:
        if args.recursive and os.path.isdir(filename):
            for root, dirs, files in os.walk(filename):
                for f in files:
                    request(os.path.join(root, f))
                for d in dirs:
                    request(os.path.join(root, d))
        else:
            request(filename)

    ret = app.exec()

    metadata_collector.close()
    vfs.close()

    del app

    return ret


def main_entrypoint():
    exit(main(sys.argv))


# EOF #
