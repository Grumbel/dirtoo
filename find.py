#!/usr/bin/python3

# dirtool.py - diff tool for directories
# Copyright (C) 2015 Ingo Ruhnke <grumbel@gmail.com>
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


import sys
import os
import argparse
import time
import fnmatch


# TODO:
# more filter expression stuff:
# os.path.isfile(path) -> isfile()
# print/formating expression


def age_in_days(filename):
    a = os.path.getmtime(filename)
    b = time.time()
    return (b - a) / (60 * 60 * 24)


def size_in_bytes(filename):
    return os.path.getsize(filename)

def name_match(filename, glob):
    return fnmatch.fnmatch(filename, glob)


def eval_filter(filter, root, filename):
    fullpath = os.path.join(root, filename)

    g = globals().copy()
    g.update({
        'p': os.path.join(root, filename),
        '_': filename,
        'daysago' : lambda f=fullpath: age_in_days(f),
        'size' : lambda f=fullpath: size_in_bytes(f),
        'name' : lambda glob, f=fullpath: name_match(f, glob),
        'iname' : lambda glob, f=fullpath: name_match(f.lower(), glob.lower()),
        'kB' : lambda s: s * 1000 * 1000,
        'KiB' : lambda s: s * 1024 * 1024,
        'MB' : lambda s: s * 1000 * 1000,
        'MiB' : lambda s: s * 1024 * 1024,
        'GB' : lambda s: s * 1000 * 1000 * 1000,
        'GiB' : lambda s: s * 1024 * 1024 * 1024,
        })
    return eval(filter, g)


def print_file(filename, null, list_verbosely):
    if null:
        sys.stdout.write(filename)
        sys.stdout.write('\0')
    elif list_verbosely:
        print("{:8} {}".format(size_in_bytes(filename), filename))
    else:
        print(filename)

def find_files(directory, filter, recursive=False, null=False, list_verbosely=False):
    for root, dirs, files in os.walk(directory):
        for f in files:
            if filter is None:
                print_file(os.path.join(root, f), null, list_verbosely)
            else:
                if eval_filter(filter, root, f):
                    print_file(os.path.join(root, f), null, list_verbosely)

        if not recursive:
            del dirs[:]


def main(argv):
    parser = argparse.ArgumentParser(description="Find files")
    parser.add_argument("DIRECTORY", nargs='*')
    parser.add_argument("-0", "--null", action="store_true",
                        help="Print \0 delimitered file list")
    parser.add_argument("-f", "--filter", metavar="EXPR", type=str,
                        help="Filter filename through EXPR")
    parser.add_argument("-r", "--recursive", action='store_true',
                        help="Recursize into the directory tree")
    parser.add_argument("-l", "--list", action='store_true',
                        help="List files verbosely")
    args = parser.parse_args(argv[1:])

    directories = args.DIRECTORY or ['.']

    for dir in directories:
        find_files(dir, args.filter, args.recursive, args.null, args.list)


if __name__ == "__main__":
    main(sys.argv)


# EOF #
