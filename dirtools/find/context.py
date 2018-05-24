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


import os
import time
import shlex
import subprocess
import random
import pwd
import grp
import stat
import datetime
import re
import hashlib
import ngram  # pylint: disable=E0401

import bytefmt

from dirtools.fuzzy import fuzzy
from dirtools.find.util import replace_item, size_in_bytes, name_match


class Context:  # pylint: disable=R0904,R0915

    def __init__(self) -> None:
        self.current_file = None

    def get_hash(self):
        return {
            '_': self.basename,
            'p': self.fullpath,
            'fullpath': self.fullpath,
            'abspath': self.fullpath,
            'ext': self.ext,
            'random': self.random,
            'rnd': self.random,
            'rand': self.random,
            'daysago': self.daysago,
            'age': self.age,
            'iso': self.iso,
            'time': self.time,
            'strftime': self.strftime,
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
            'fuzzy': self.fuzzy,
            'ngram': self.ngram,
            'regex': self.regex,
            'iregex': self.iregex,
            'rx': self.regex,
            'irx': self.iregex,
            're': self.regex,
            'ire': self.iregex,
            'ascii': self.ascii,
            'atime': self.atime,
            'ctime': self.ctime,
            'mtime': self.mtime,
            'uid': self.uid,
            'gid': self.gid,
            'owner': self.owner,
            'group': self.group,
            'mode': self.mode,
            'modehr': self.modehr,
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
            'in_TiB': self.in_TiB,

            'sha1': self.sha1,
            'md5': self.md5,
        }

    def random(self, p=0.5):
        return random.random() < p

    def basename(self):
        return os.path.basename(self.current_file)

    def fullpath(self):
        return self.current_file

    def ext(self):
        _, ext = os.path.splitext(self.current_file)
        return ext

    def sha1(self):
        sha1 = hashlib.sha1()
        with open(self.current_file, 'rb') as fin:
            data = fin.read(65536)
            while data:
                sha1.update(data)
                data = fin.read(65536)

        return sha1.hexdigest()

    def md5(self):
        md5 = hashlib.md5()
        with open(self.current_file, 'rb') as fin:
            data = fin.read(65536)
            while data:
                md5.update(data)
                data = fin.read(65536)

        return md5.hexdigest()

    def age(self):
        a = os.path.getmtime(self.current_file)
        b = time.time()
        return b - a

    def iso(self, t=None):
        if t is None:
            t = self.mtime()
        return datetime.datetime.fromtimestamp(t).strftime("%F")

    def time(self, t=None):
        if t is None:
            t = self.mtime()
        return datetime.datetime.fromtimestamp(t).strftime("%H:%M")

    def strftime(self, fmt, t=None):
        if t is None:
            t = self.mtime()
        return datetime.datetime.fromtimestamp(t).strftime(fmt)

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

    def sizehr(self, s=None, style="decimal", compact=False):
        """Returns size() formated a human readable string"""

        if s is None:
            s = self.size()

        return bytefmt.humanize(s, style=style, compact=compact)

    def size(self):
        return size_in_bytes(self.current_file)

    def name(self, glob):
        return name_match(self.current_file, glob)

    def regex(self, regex, path=None):
        if path is None:
            path = self.basename()
        return re.search(regex, path)

    def iregex(self, regex, path=None):
        if path is None:
            path = self.basename()
        return re.search(regex, path, flags=re.IGNORECASE)

    def iname(self, glob):
        return name_match(self.current_file.lower(), glob.lower())

    def ngram(self, text, threshold=0.15):
        return ngram.NGram.compare(os.path.basename(self.current_file).lower(), text.lower()) >= threshold

    def fuzzy(self, text, threshold=0.5, n=3):
        neddle = text.lower()
        haystack = os.path.basename(self.current_file).lower()
        return fuzzy(neddle, haystack, n=n) >= threshold

    def ascii(self):
        filename = os.path.basename(self.current_file)
        try:
            filename.encode("ascii")
        except UnicodeError:
            return False
        return True

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

    def modehr(self):  # pylint: disable=R0912
        mode = os.lstat(self.current_file).st_mode

        s = ""

        if stat.S_ISDIR(mode):
            s += "d"
        elif stat.S_ISCHR(mode):
            s += "c"
        elif stat.S_ISBLK(mode):
            s += "b"
        elif stat.S_ISREG(mode):
            s += "-"
        elif stat.S_ISFIFO(mode):
            s += "p"
        elif stat.S_ISLNK(mode):
            s += "l"
        elif stat.S_ISSOCK(mode):
            s += "s"
        else:
            s += "?"

        if mode & stat.S_IRUSR:
            s += "r"
        else:
            s += "-"

        if mode & stat.S_IWUSR:
            s += "w"
        else:
            s += "-"

        if mode & stat.S_IXUSR:
            s += "s" if mode & stat.S_ISGID else "x"
        else:
            s += "S" if mode & stat.S_ISGID else "-"

        if mode & stat.S_IRGRP:
            s += "r"
        else:
            s += "-"
        if mode & stat.S_IWGRP:
            s += "w"
        else:
            s += "-"

        if mode & stat.S_IXGRP:
            s += "s" if mode & stat.S_ISGID else "x"
        else:
            s += "S" if mode & stat.S_ISGID else "-"

        if mode & stat.S_IROTH:
            s += "r"
        else:
            s += "-"

        if mode & stat.S_IWOTH:
            s += "w"
        else:
            s += "-"

        if mode & stat.S_IXOTH:  # stat.S_ISVTX:
            s += "t" if mode & stat.S_ISGID else "x"
        else:
            s += "T" if mode & stat.S_ISGID else "-"

        return s

    def ino(self):
        return os.lstat(self.current_file).st_ino

    def kB(self, s):  # noqa: N802
        return s * 1000

    def KiB(self, s):  # noqa: N802
        return s * 1024

    def MB(self, s):  # noqa: N802
        return s * 1000 ** 2

    def MiB(self, s):  # noqa: N802
        return s * 1024 ** 2

    def GB(self, s):  # noqa: N802
        return s * 1000 ** 3

    def GiB(self, s):  # noqa: N802
        return s * 1024 ** 3

    def TB(self, s):  # noqa: N802
        return s * 1000 ** 4

    def TiB(self, s):  # noqa: N802
        return s * 1024 ** 4

    def in_kB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000

    def in_KiB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024

    def in_MB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000 ** 2

    def in_MiB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024 ** 2

    def in_GB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000 ** 3

    def in_GiB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024 ** 3

    def in_TB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000 ** 4

    def in_TiB(self, s=None):  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024 ** 4


# EOF #
