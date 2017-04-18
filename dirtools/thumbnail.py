# dirtool.py - diff tool for directories
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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
import hashlib
import urllib
import xdg.BaseDirectory


def make_thumbnail_filename(filename, flavor="normal"):
    url = "file://" + urllib.parse.quote(os.path.abspath(filename))
    digest = hashlib.md5(os.fsencode(url)).hexdigest()
    result = os.path.join(xdg.BaseDirectory.xdg_cache_home, "thumbnails", flavor, digest + ".png")
    print(result)
    if os.path.exists(result):
        return result
    else:
        return None


# EOF #
