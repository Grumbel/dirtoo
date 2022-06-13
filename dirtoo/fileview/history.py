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


from typing import List, Optional

import logging
import sqlite3
import time

from dirtoo.fileview.location import Location

logger = logging.getLogger(__name__)


class SqlHistory:

    def __init__(self, filename: str) -> None:
        self._db_filename = filename
        self._db = sqlite3.connect(self._db_filename, isolation_level=None)
        self._init_db()

    def close(self):
        self._db.commit()
        self._db.close()

    def _init_db(self):
        self._db.execute("CREATE TABLE IF NOT EXISTS history ("
                         "group_id INTEGER, "
                         "date REAL, "
                         "location TEXT)")

    def get_entries(self, limit: Optional[int] = None) -> List[Location]:
        c = self._db.cursor()
        c.execute("SELECT group_id, date, location "
                  "FROM history "
                  "ORDER BY date DESC" +
                  (" LIMIT {}".format(limit) if limit is not None else ""))

        results: List[Location] = []
        for group_id, date, location_url in c.fetchall():
            try:
                loc = Location.from_url(location_url)
            except Exception:
                logging.exception("Location parsing failed")
            else:
                results.append(loc)

        return results

    def get_unique_entries(self, limit: Optional[int] = None) -> List[Location]:
        entries: List[Location] = []
        for entry in self.get_entries(1000):
            if entry not in entries:
                entries.append(entry)
                if limit is not None and len(entries) >= limit:
                    break
        return entries

    def append(self, location: Location) -> None:
        insertion_time = time.time()
        c = self._db.cursor()
        c.execute("BEGIN")
        group_id = (c.execute("SELECT max(group_id) FROM history").fetchone()[0] or 0) + 1
        c.execute("INSERT INTO history (group_id, date, location) VALUES (?, ?, ?)",
                  (group_id, insertion_time, location.as_url()))
        c.execute("COMMIT")
        self._db.commit()

    def append_group(self, locations: List[Location]) -> None:
        insertion_time = time.time()

        c = self._db.cursor()
        c.execute("BEGIN")
        group_id = (c.execute("SELECT max(group_id) FROM history").fetchone()[0] or 0) + 1
        values = [(group_id, insertion_time, location.as_url()) for location in locations]
        c.executemany("INSERT INTO history (group_id, date, location) VALUES (?, ?, ?)",
                      values)
        c.execute("COMMIT")
        self._db.commit()


# EOF #
