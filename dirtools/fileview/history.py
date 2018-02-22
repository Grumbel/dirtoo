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


from typing import List

import logging

from dirtools.util import remove_duplicates
from dirtools.fileview.location import Location

logger = logging.getLogger(__name__)


class History:

    def __init__(self, filename: str) -> None:
        self.config_filename = filename

    def get_entries(self) -> List[Location]:
        try:
            with open(self.config_filename, "r") as fin:
                lines = fin.read().splitlines()

                results: List[Location] = []
                for line in lines:
                    try:
                        results.append(Location.from_url(line))
                    except Exception as err:
                        logger.warning("ignoring history entry: %s", line)

            return remove_duplicates(results)

        except FileNotFoundError:
            return []

    def append(self, location: Location) -> None:
        with open(self.config_filename, "a") as fout:
            print(location.as_url(), file=fout)


# EOF #
