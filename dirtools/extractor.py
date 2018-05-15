# dirtool.py - diff tool for directories
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


from abc import ABC, abstractmethod


class ExtractorResult:

    SUCCESS = 0
    FAILURE = 1
    WORKING = 2

    @staticmethod
    def success(message=""):
        return ExtractorResult(ExtractorResult.SUCCESS, message)

    @staticmethod
    def failure(message):
        return ExtractorResult(ExtractorResult.FAILURE, message)

    def __init__(self, status, message=""):
        self.status = status
        self.message = message

    def __str__(self):
        return "ExtractorResult({}, \"{}\")".format(self.status, self.message)


class Extractor(ABC):

    @abstractmethod
    def sig_entry_extracted(self):
        pass

    @abstractmethod
    def sig_finished(self):
        pass

    @abstractmethod
    def extract(self, outdir: str) -> None:
        pass


# EOF #
