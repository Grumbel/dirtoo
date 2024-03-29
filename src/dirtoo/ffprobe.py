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


import os
import subprocess
import json


class FFProbe:

    def __init__(self, filename: str) -> None:
        self._ffprobe = os.environ.get("DIRTOO_FFPROBE") or "ffprobe"
        with subprocess.Popen([self._ffprobe,
                               '-v', 'error',
                               '-print_format', 'json',
                               '-show_format',
                               '-show_streams',
                               filename],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE) as proc:
            try:
                out_bytes, err_bytes = proc.communicate(timeout=15)
            except subprocess.TimeoutExpired as exc:
                proc.kill()
                out_bytes, err_bytes = proc.communicate()
                out = out_bytes.decode()
                err = err_bytes.decode()
                raise RuntimeError("FFProbe: timeout: {}".format(filename)) from exc

            out = out_bytes.decode()
            err = err_bytes.decode()

            if proc.returncode != 0:
                raise RuntimeError("FFProbe: {}: {}: {}".format(filename, proc.returncode, err))

        self.js = json.loads(out)
        self.streams = self.js['streams']
        self.format = self.js['format']

    def duration(self) -> float:
        return float(self.format['duration']) * 1000

    def width(self) -> int:
        return int(self.streams[0]['width'])

    def height(self) -> int:
        return int(self.streams[0]['height'])

    def framerate(self) -> float:
        text = self.streams[0]['avg_frame_rate']
        numerator, denominator = text.split("/")
        return int(numerator) / int(denominator)


# EOF #
