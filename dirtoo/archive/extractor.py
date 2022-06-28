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


from PyQt5.QtCore import QObject, pyqtSignal
from abc import abstractmethod
from enum import Enum


class ExtractorResultStatus(Enum):

    SUCCESS = 0
    FAILURE = 1
    WORKING = 2


class ExtractorResult:

    @staticmethod
    def success(message: str = "") -> 'ExtractorResult':
        return ExtractorResult(ExtractorResultStatus.SUCCESS, message)

    @staticmethod
    def failure(message: str) -> 'ExtractorResult':
        return ExtractorResult(ExtractorResultStatus.FAILURE, message)

    def __init__(self, status: ExtractorResultStatus, message: str = "") -> None:
        self.status = status
        self.message = message

    def __str__(self) -> str:
        return "ExtractorResult({}, \"{}\")".format(self.status, self.message)


class Extractor(QObject):

    def __init__(self) -> None:
        super().__init__()

    @property
    @abstractmethod
    def sig_entry_extracted(self) -> pyqtSignal:
        pass

    @property
    @abstractmethod
    def sig_finished(self) -> pyqtSignal:
        pass

    @abstractmethod
    def extract(self) -> ExtractorResult:
        pass

    def interrupt(self) -> None:
        pass


def make_extractor(filename: str, outdir: str) -> Extractor:
    from dirtoo.archive.rar_extractor import RarExtractor
    from dirtoo.archive.sevenzip_extractor import SevenZipExtractor
    from dirtoo.archive.libarchive_extractor import LibArchiveExtractor

    # FIXME: Use mime-type to decide proper extractor
    if filename.lower().endswith(".rar"):
        extractor = RarExtractor(filename, outdir)
    elif True:  # pylint: disable=using-constant-test
        extractor = SevenZipExtractor(filename, outdir)
    else:
        extractor = LibArchiveExtractor(filename, outdir)  # type: ignore

    return extractor


# EOF #
