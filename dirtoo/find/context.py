# dirtoo - File and directory manipulation tools for Python
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


from typing import cast, Callable, Dict, Optional

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

from dirtoo.fuzzy import fuzzy
from dirtoo.find.util import replace_item, size_in_bytes, name_match


class Context:  # pylint: disable=R0904,R0915

    def __init__(self) -> None:
        self.current_file: Optional[str] = None

    def get_hash(self) -> Dict[str, Callable]:
        return {
            '_': self.basename,
            'p': self.fullpath,
            'fullpath': self.fullpath,
            'qfullpath': self.qfullpath,
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

    def random(self, p: float = 0.5) -> bool:
        return random.random() < p

    def basename(self) -> str:
        assert self.current_file is not None
        return os.path.basename(self.current_file)

    def fullpath(self) -> str:
        assert self.current_file is not None
        return self.current_file

    def qfullpath(self) -> str:
        assert self.current_file is not None

        def quote_or_pad(text: str) -> str:
            result = shlex.quote(text)
            if result[0] == "'":
                return result
            else:
                return " " + result

        return quote_or_pad(self.current_file)

    def ext(self) -> str:
        assert self.current_file is not None

        _, ext = os.path.splitext(self.current_file)
        return ext

    def sha1(self) -> str:
        assert self.current_file is not None

        sha1 = hashlib.sha1()
        with open(self.current_file, 'rb') as fin:
            data = fin.read(65536)
            while data:
                sha1.update(data)
                data = fin.read(65536)

        return sha1.hexdigest()

    def md5(self) -> str:
        assert self.current_file is not None

        md5 = hashlib.md5()
        with open(self.current_file, 'rb') as fin:
            data = fin.read(65536)
            while data:
                md5.update(data)
                data = fin.read(65536)

        return md5.hexdigest()

    def age(self) -> float:
        assert self.current_file is not None

        a = os.path.getmtime(self.current_file)
        b = time.time()
        return b - a

    def iso(self, t: Optional[float] = None) -> str:
        if t is None:
            t = self.mtime()
        return datetime.datetime.fromtimestamp(t).strftime("%F")

    def time(self, t: Optional[float] = None) -> str:
        if t is None:
            t = self.mtime()
        return datetime.datetime.fromtimestamp(t).strftime("%H:%M")

    def strftime(self, fmt: str, t: Optional[float] = None) -> str:
        if t is None:
            t = self.mtime()
        return datetime.datetime.fromtimestamp(t).strftime(fmt)

    def stdout(self, exec_str: str) -> str:
        cmd = shlex.split(exec_str)

        # FIXME: can't use {} inside format str: unexpected '{' in field name
        cmd = replace_item(cmd, "(FILE)", [self.current_file])

        return subprocess.check_output(cmd).decode('utf-8').strip()

    def sec(self, s: float) -> float:
        return s

    def min(self, m: float) -> float:
        return m * 60

    def hour(self, h: float) -> float:
        return h * 60 * 60

    def day(self, days: float) -> float:
        return days * 60 * 60 * 24

    def week(self, weeks: float) -> float:
        return weeks * 60 * 60 * 24 * 7

    def month(self, month: float) -> float:
        return month * 60 * 60 * 24 * 7 * 30.4368

    def year(self, years: float) -> float:
        return years * 60 * 60 * 24 * 7 * 30.4368 * 12

    def in_sec(self, sec: float) -> float:
        return sec

    def in_mins(self, sec: float) -> float:
        return sec / 60

    def in_hours(self, sec: float) -> float:
        return sec / 60 / 60

    def in_days(self, sec: float) -> float:
        return sec / 60 / 60 / 24

    def in_weeks(self, sec: float) -> float:
        return sec / 60 / 60 / 24 / 7

    def in_month(self, sec: float) -> float:
        return sec / 60 / 60 / 24 / 7 / 30.4368

    def in_years(self, sec: float) -> float:
        return sec / 60 / 60 / 24 / 7 / 30.4368 / 12

    def daysago(self) -> float:
        assert self.current_file is not None

        a = os.path.getmtime(self.current_file)
        b = time.time()
        return (b - a) / (60 * 60 * 24)

    def sizehr(self, s: int = None, style: str = "decimal", compact: bool = False) -> str:
        """Returns size() formated a human readable string"""

        if s is None:
            s = self.size()

        return cast(str, bytefmt.humanize(s, style=style, compact=compact))  # FIXME: why is the cast necessary?

    def size(self) -> int:
        assert self.current_file is not None
        return size_in_bytes(self.current_file)

    def name(self, glob: str) -> bool:
        assert self.current_file is not None
        return name_match(self.current_file, glob)

    def regex(self, regex: str, path: Optional[str] = None) -> bool:
        if path is None:
            path = self.basename()
        return bool(re.search(regex, path))

    def iregex(self, regex: str, path: Optional[str] = None) -> bool:
        if path is None:
            path = self.basename()
        return bool(re.search(regex, path, flags=re.IGNORECASE))

    def iname(self, glob: str) -> bool:
        assert self.current_file is not None
        return name_match(self.current_file.lower(), glob.lower())

    def ngram(self, text: str, threshold: float = 0.15) -> bool:
        assert self.current_file is not None
        return bool(ngram.NGram.compare(os.path.basename(self.current_file).lower(), text.lower()) >= threshold)

    def fuzzy(self, text: str, threshold: float = 0.5, n: int = 3) -> bool:
        assert self.current_file is not None
        neddle = text.lower()
        haystack = os.path.basename(self.current_file).lower()
        return fuzzy(neddle, haystack, n=n) >= threshold

    def ascii(self) -> bool:
        assert self.current_file is not None
        filename = os.path.basename(self.current_file)
        try:
            filename.encode("ascii")
        except UnicodeError:
            return False
        return True

    def atime(self) -> float:
        assert self.current_file is not None
        return os.lstat(self.current_file).st_atime

    def ctime(self) -> float:
        assert self.current_file is not None
        return os.lstat(self.current_file).st_ctime

    def mtime(self) -> float:
        assert self.current_file is not None
        return os.lstat(self.current_file).st_mtime

    def uid(self) -> int:
        assert self.current_file is not None
        return os.lstat(self.current_file).st_uid

    def gid(self) -> int:
        assert self.current_file is not None
        return os.lstat(self.current_file).st_gid

    def owner(self) -> str:
        assert self.current_file is not None
        return pwd.getpwuid(os.lstat(self.current_file).st_uid).pw_name

    def group(self) -> str:
        assert self.current_file is not None
        return grp.getgrgid(os.lstat(self.current_file).st_gid).gr_name

    def isblk(self) -> bool:
        assert self.current_file is not None
        return stat.S_ISBLK(os.lstat(self.current_file).st_mode)

    def islnk(self) -> bool:
        assert self.current_file is not None
        return stat.S_ISLNK(os.lstat(self.current_file).st_mode)

    def isdir(self) -> bool:
        assert self.current_file is not None
        return stat.S_ISDIR(os.lstat(self.current_file).st_mode)

    def ischr(self) -> bool:
        assert self.current_file is not None
        return stat.S_ISCHR(os.lstat(self.current_file).st_mode)

    def isfifo(self) -> bool:
        assert self.current_file is not None
        return stat.S_ISFIFO(os.lstat(self.current_file).st_mode)

    def isreg(self) -> bool:
        assert self.current_file is not None
        return stat.S_ISREG(os.lstat(self.current_file).st_mode)

    def mode(self) -> int:
        assert self.current_file is not None
        return stat.S_IMODE(os.lstat(self.current_file).st_mode)

    def modehr(self) -> str:  # pylint: disable=R0912
        assert self.current_file is not None
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

    def ino(self) -> int:
        assert self.current_file is not None
        return os.lstat(self.current_file).st_ino

    def kB(self, s: int) -> int:  # noqa: N802
        return s * 1000

    def KiB(self, s: int) -> int:  # noqa: N802
        return s * 1024

    def MB(self, s: int) -> int:  # noqa: N802
        return s * 1000 ** 2

    def MiB(self, s: int) -> int:  # noqa: N802
        return s * 1024 ** 2

    def GB(self, s: int) -> int:  # noqa: N802
        return s * 1000 ** 3

    def GiB(self, s: int) -> int:  # noqa: N802
        return s * 1024 ** 3

    def TB(self, s: int) -> int:  # noqa: N802
        return s * 1000 ** 4

    def TiB(self, s: int) -> int:  # noqa: N802
        return s * 1024 ** 4

    def in_kB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000

    def in_KiB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024

    def in_MB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000 ** 2

    def in_MiB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024 ** 2

    def in_GB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000 ** 3

    def in_GiB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024 ** 3

    def in_TB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1000 ** 4

    def in_TiB(self, s: Optional[int] = None) -> float:  # noqa: N802
        if s is None:
            s = self.size()
        return s / 1024 ** 4


# EOF #
