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
import random
import pwd
import grp
import stat
import string
import datetime


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

    def __init__(self, fmt_str, finisher=False):
        super().__init__()

        self.fmt_str = fmt_str
        self.finisher = finisher

        self.file_count = 0
        self.size_total = 0

        self.ctx = Context()
        self.global_vars = globals().copy()
        self.global_vars.update(self.ctx.get_hash())

    def file(self, root, filename):
        self.file_count += 1

        fullpath = os.path.join(root, filename)

        byte_size = os.lstat(fullpath).st_size

        self.size_total += byte_size

        self.ctx.current_file = fullpath

        local_vars = {
            'p': fullpath,
            '_': os.path.basename(filename)
            }

        fmt = string.Formatter()
        for (literal_text, field_name, format_spec, conversion) in fmt.parse(self.fmt_str):
            if literal_text is not None:
                sys.stdout.write(literal_text)

            if field_name is not None:
                value = eval(field_name, self.global_vars, local_vars)
                sys.stdout.write(format(value, format_spec))

    def finish(self):
        if self.finisher:
            print("-" * 72)
            print("{:>12}  {} files in total".format(self.ctx.sizehr(self.size_total), self.file_count))


class MultiAction(Action):

    def __init__(self):
        super().__init__()

        self.actions = []

    def add(self, action):
        self.actions.append(action)

    def file(self, root, filename):
        for action in self.actions:
            action.file(root, filename)

    def directory(self, root, filename):
        for action in self.actions:
            action.directory(root, filename)

    def finish(self):
        for action in self.actions:
            action.finish()


class ExecAction(Action):

    def __init__(self, exec_str):
        super().__init__()

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


class NoFilter:

    def __init__(self):
        pass

    def match_file(self, root, filename):
        return True


class Context:

    def __init__(self):
        self.current_file = None

    def get_hash(self):
        return {
            '_': self.basename,
            'p': self.fullpath,
            'fullpath': self.fullpath,
            'abspath': self.fullpath,
            'random': self.random,
            'rnd': self.random,
            'rand': self.random,
            'daysago': self.daysago,
            'age': self.age,
            'iso': self.iso,
            'stdout': self.stdout,

            'sec': self.sec,
            'min': self.min,
            'hour': self.hour,
            'hours': self.hour,
            'days': self.day,
            'day': self.day,
            'week': self.week,
            'weeks': self.week,
            'month': self.month,
            'year': self.year,
            'years': self.year,

            'in_sec': self.in_sec,
            'in_min': self.in_mins,
            'in_mins': self.in_mins,
            'in_hours': self.in_hours,
            'in_days': self.in_days,
            'in_weeks': self.in_weeks,
            'in_month': self.in_month,
            'in_years': self.in_years,

            'sizehr': self.sizehr,
            'size': self.size,
            'name': self.name,
            'iname': self.iname,
            'atime': self.atime,
            'ctime': self.ctime,
            'mtime': self.mtime,
            'uid': self.uid,
            'gid': self.gid,
            'owner': self.owner,
            'group': self.group,
            'mode': self.mode,
            'isblk': self.isblk,
            'islnk': self.islnk,
            'islink': self.islnk,
            'isdir': self.isdir,
            'ischr': self.ischr,
            'isfifo': self.isfifo,
            'isreg': self.isreg,
            'ino': self.ino,

            'kB': self.kB,
            'KiB': self.KiB,
            'MB': self.MB,
            'MiB': self.MiB,
            'GB': self.GB,
            'GiB': self.GiB,
            'TB': self.TB,
            'TiB': self.TiB,

            'in_kB': self.in_kB,
            'in_KiB': self.in_KiB,
            'in_MB': self.in_MB,
            'in_MiB': self.in_MiB,
            'in_GB': self.in_GB,
            'in_GiB': self.in_GiB,
            'in_TB': self.in_TB,
            'in_TiB': self.in_TiB
            }

    def random(self, p=0.5):
        return random.random() < p

    def basename(self):
        return os.path.basename(self.current_file)

    def fullpath(self):
        return self.current_file

    def age(self):
        a = os.path.getmtime(self.current_file)
        b = time.time()
        return b - a

    def iso(self, t=None):
        if t is None:
            t = self.mtime()
        return datetime.date.fromtimestamp(t).isoformat()

    def stdout(self, exec_str):
        cmd = shlex.split(exec_str)

        # FIXME: can't use {} inside format str: unexpected '{' in field name
        cmd = replace_item(cmd, "(FILE)", [self.current_file])

        return subprocess.check_output(cmd).decode('utf-8').strip()

    def sec(self, s):
        return s

    def min(self, m):
        return m * 60

    def hour(self, h):
        return h * 60 * 60

    def day(self, days):
        return days * 60 * 60 * 24

    def week(self, weeks):
        return weeks * 60 * 60 * 24 * 7

    def month(self, month):
        return month * 60 * 60 * 24 * 7 * 30.4368

    def year(self, years):
        return years * 60 * 60 * 24 * 7 * 30.4368 * 12

    def in_sec(self, sec):
        return sec

    def in_mins(self, sec):
        return sec / 60

    def in_hours(self, sec):
        return sec / 60 / 60

    def in_days(self, sec):
        return sec / 60 / 60 / 24

    def in_weeks(self, sec):
        return sec / 60 / 60 / 24 / 7

    def in_month(self, sec):
        return sec / 60 / 60 / 24 / 7 / 30.4368

    def in_years(self, sec):
        return sec / 60 / 60 / 24 / 7 / 30.4368 / 12

    def daysago(self):
        a = os.path.getmtime(self.current_file)
        b = time.time()
        return (b - a) / (60 * 60 * 24)

    def sizehr(self, s=None):
        """Returns size() formated a human readable string"""

        if s is None:
            s = self.size()

        if s < 1000:
            return "{}B".format(s)
        elif s < 1000 ** 2:
            return "{:.2f}kB".format(self.in_kB(s))
        elif s < 1000 ** 3:
            return "{:.2f}MB".format(self.in_MB(s))
        elif s < 1000 ** 4:
            return "{:.2f}GB".format(self.in_GB(s))
        else:
            return "{:.2f}TB".format(self.in_TB(s))

    def size(self):
        return size_in_bytes(self.current_file)

    def name(self, glob):
        return name_match(self.current_file, glob)

    def iname(self, glob):
        return name_match(self.current_file.lower(), glob.lower())

    def atime(self):
        return os.lstat(self.current_file).st_atime

    def ctime(self):
        return os.lstat(self.current_file).st_ctime

    def mtime(self):
        return os.lstat(self.current_file).st_mtime

    def uid(self):
        return os.lstat(self.current_file).st_uid

    def gid(self):
        return os.lstat(self.current_file).st_gid

    def owner(self):
        return pwd.getpwuid(os.lstat(self.current_file).st_uid).pw_name

    def group(self):
        return grp.getgrgid(os.lstat(self.current_file).st_gid).gr_name

    def isblk(self):
        return stat.S_ISBLK(os.lstat(self.current_file).st_mode)

    def islnk(self):
        return stat.S_ISLNK(os.lstat(self.current_file).st_mode)

    def isdir(self):
        return stat.S_ISDIR(os.lstat(self.current_file).st_mode)

    def ischr(self):
        return stat.S_ISCHR(os.lstat(self.current_file).st_mode)

    def isfifo(self):
        return stat.S_ISFIFO(os.lstat(self.current_file).st_mode)

    def isreg(self):
        return stat.S_ISREG(os.lstat(self.current_file).st_mode)

    def mode(self):
        return stat.S_IMODE(os.lstat(self.current_file).st_mode)

    def ino(self):
        return os.lstat(self.current_file).st_ino

    def kB(self, s):
        return s * 1000

    def KiB(self, s):
        return s * 1024

    def MB(self, s):
        return s * 1000 ** 2

    def MiB(self, s):
        return s * 1024 ** 2

    def GB(self, s):
        return s * 1000 ** 3

    def GiB(self, s):
        return s * 1024 ** 3

    def TB(self, s):
        return s * 1000 ** 4

    def TiB(self, s):
        return s * 1024 ** 4

    def in_kB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1000

    def in_KiB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1024

    def in_MB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1000 ** 2

    def in_MiB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1024 ** 2

    def in_GB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1000 ** 3

    def in_GiB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1024 ** 3

    def in_TB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1000 ** 4

    def in_TiB(self, s=None):
        if s is None:
            s = self.size()
        return s / 1024 ** 4


class ExprFilter:

    def __init__(self, expr):
        self.expr = expr
        self.local_vars = {}
        self.ctx = Context()
        self.global_vars = globals().copy()
        self.global_vars.update(self.ctx.get_hash())

    def match_file(self, root, filename):
        fullpath = os.path.join(root, filename)

        self.ctx.current_file = fullpath
        local_vars = {
            'p': fullpath,
            '_': filename
            }
        result = eval(self.expr, self.global_vars, local_vars)
        return result


def size_in_bytes(filename):
    return os.lstat(filename).st_size


def name_match(filename, glob):
    return fnmatch.fnmatch(filename, glob)


def find_files(directory, recursive, filter, action):
    result = []

    for root, dirs, files in os.walk(directory):
        for f in files:
            if filter.match_file(root, f):
                action.file(root, f)
                fullpath = os.path.join(root, f)
                result.append(fullpath)

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
    parser.add_argument("-q", "--quiet", action='store_true',
                        help="Be quiet")
    parser.add_argument("--exec", metavar="EXEC",
                        help="Execute EXEC")

    return parser.parse_args(args)


def create_action(args):
    action = MultiAction()

    if args.quiet:
        pass
    elif args.null:
        action.add(PrinterAction("{fullpath()}\0"))
    elif args.list:
        action.add(PrinterAction("{size():12}  {fullpath()}\n", finisher=True))
    elif args.print:
        action.add(PrinterAction(args.print))
    else:
        action.add(PrinterAction("{fullpath()}\n"))

    if args.exec:
        action.add(ExecAction(args.exec))

    return action


def create_filter(args):
    if args.filter:
        return ExprFilter(args.filter)
    else:
        return NoFilter()


def main(argv):
    args = parse_args(argv[1:])

    directories = args.DIRECTORY or ['.']

    find_action = create_action(args)
    find_filter = create_filter(args)

    for d in directories:
        find_files(d, args.recursive, find_filter, find_action)

    find_action.finish()


if __name__ == "__main__":
    main(sys.argv)


# EOF #
