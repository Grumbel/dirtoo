#!/usr/bin/env python3

# dirtool.py - diff tool for directories
# Copyright (C) 2014 Ingo Ruhnke <grumbel@gmail.com>
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


import os
import argparse
import functools
import shutil
import filecmp
from itertools import chain
import os
import json
import os
from os.path import join, getsize
import logging
import hashlib

# {"name":"dirtool.py","asize":9848,"dsize":12288,"ino":4560601}
# {}

# give absolute path for directory
# local name for files

def hashfile(fin, hasher, blocksize=65536):
    buf = fin.read(blocksize)
    while len(buf) > 0:
        hasher.update(buf)
        buf = fin.read(blocksize)
    return hasher.hexdigest()


def process_directory(directory):
    for root, dirs, files in os.walk(directory):
        print("FILES:", files)
        for f in files:
            filename = os.path.join(root, f)
            if os.path.isfile(filename):
                with open(filename, "rb") as fin:
                    sha1 = hashfile(fin, hashlib.sha1())
                logging.info("regular file %s %s", filename, sha1)
            elif os.path.islink(filename):
                logging.info("symlink file %s", filename)
            else:
                logging.info("ignore %s", filename)

            statinfo = os.lstat(filename)
        print("}")


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)
    parser = argparse.ArgumentParser(description='dirtool')
    parser.add_argument('DIRECTORY', action='store', type=str, nargs='+',
                        help='directories to scan')
    parser.add_argument('-o', '--output', type=str, help='output file')
    args = parser.parse_args()

    for directory in args.DIRECTORY:
        process_directory(directory)

# EOF #
