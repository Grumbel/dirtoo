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
import shlex
import subprocess


# TODO:
# more filter expression stuff:
# os.path.isfile(path) -> isfile()
# print/formating expression

class Action:

    def __init__(self):
        pass

    def file(self, root, filename):
        pass

    def directory(self, root, filename):
        pass

    def finish(self):
        pass


class PrinterAction(Action):

    def __init__(self, fmt, finisher=False):
        self.fmt = fmt
        self.finisher = finisher

        self.file_count = 0
        self.size_total = 0

    def file(self, root, filename):
        self.file_count += 1

        fullpath = os.path.join(root, filename)

        byte_size = os.lstat(fullpath).st_size

        self.size_total += byte_size

        values = {
            's': byte_size,
            'size': byte_size,
            'kB': byte_size / 1000 ,
            'KiB': byte_size / 1024,
            'MB': byte_size / 1000 / 1000,
            'MiB': byte_size / 1024 / 1024,
            'GB': byte_size / 1000 / 1000 / 1000,
            'GiB': byte_size / 1024 / 1024 / 1024,
            'p': fullpath,
            'r': root,
            'root': root,
            '_': filename,
            'fullpath': fullpath
            }

        line = self.fmt.format(**values)
        sys.stdout.write(line)

    def finish(self):
        if self.finisher:
            print("-" * 72)
            print("{} {} files in total".format(self.size_total, self.file_count))


class ExecAction(Action):

    def __init__(self, exec_str):
        self.on_file_cmd = None
        self.on_multi_cmd = None
        self.all_files = []

        cmd = shlex.split(exec_str)

        if "{}+" in cmd:
            self.on_multi_cmd = cmd
        elif "{}" in cmd:
            self.on_file_cmd = cmd
        else:
            pass  # FIXME

    def file(self, root, name):
        if self.on_file_cmd:
            cmd = replace_item(self.on_file_cmd, "{}", [os.path.join(root, name)])
            subprocess.call(cmd)

        if self.on_multi_cmd:
            self.all_files.append(os.path.join(root, name))

    def directory(self, root, name):
        pass

    def finish(self):
        if self.on_multi_cmd:
            multi_cmd = replace_item(self.on_multi_cmd, "{}+", self.all_files)
            subprocess.call(multi_cmd)


def age_in_days(filename):
    a = os.path.getmtime(filename)
    b = time.time()
    return (b - a) / (60 * 60 * 24)


def size_in_bytes(filename):
    return os.lstat(filename).st_size

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


def find_files(directory, filter, recursive, action):
    result = []

    for root, dirs, files in os.walk(directory):
        for f in files:
            if filter is None:
                action.file(root, f)
                result.append(os.path.join(root, f))
            else:
                if eval_filter(filter, root, f):
                    action.file(root, f)
                    result.append(os.path.join(root, f))

        if not recursive:
            del dirs[:]

    return result


def replace_item(lst, needle, replacements):
    result = []
    for i in lst:
        if i == needle:
            result += replacements
        else:
            result.append(i)
    return result


def parse_args(args):
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
    parser.add_argument("-p", "--print", metavar="FMT",
                        help="List files with the given format string")
    parser.add_argument("--exec", metavar="EXEC",
                        help="Execute EXEC")

    return parser.parse_args(args)


def create_action(args):
    if args.exec:
        action = ExecAction(args.exec)
    elif args.null:
        action = PrinterAction("{fullpath}\0")
    elif args.list:
        action = PrinterAction("{size} {fullpath}\n", finisher=True)
    elif args.print:
        action = PrinterAction(args.print)
    else:
        action = PrinterAction("{fullpath}\n")

    return action

def main(argv):
    args = parse_args(argv[1:])

    directories = args.DIRECTORY or ['.']

    action = create_action(args)

    results = []
    for dir in directories:
        results += find_files(dir, args.filter, args.recursive, action)

    action.finish()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
