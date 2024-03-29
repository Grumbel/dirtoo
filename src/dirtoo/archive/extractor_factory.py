# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018-2022 Ingo Ruhnke <grumbel@gmail.com>
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


from dirtoo.archive.extractor import Extractor
from dirtoo.archive.rar_extractor import RarExtractor
from dirtoo.archive.sevenzip_extractor import SevenZipExtractor
from dirtoo.archive.libarchive_extractor import LibArchiveExtractor


def make_extractor(filename: str, outdir: str) -> Extractor:
    # FIXME: Use mime-type to decide proper extractor
    extractor: Extractor
    if filename.lower().endswith(".rar") and RarExtractor.is_available():
        extractor = RarExtractor(filename, outdir)
    elif True:  # pylint: disable=using-constant-test
        extractor = SevenZipExtractor(filename, outdir)
    else:
        extractor = LibArchiveExtractor(filename, outdir)  # type: ignore

    return extractor


# EOF #
