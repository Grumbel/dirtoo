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


from typing import cast, Union, Dict, Any

import hashlib
import json
import logging
import os
import urllib.parse
import xdg.BaseDirectory


class MetaDataCache:

    def __init__(self):
        self.directory = os.path.join(xdg.BaseDirectory.xdg_cache_home, "dt-fileview", "metadata")
        logging.info("MetaDataCache.__init__: %s", self.directory)
        try:
            os.makedirs(self.directory)
        except FileExistsError:
            pass

    def _make_filename(self, abspath: str) -> str:
        url = "file://" + urllib.parse.quote(abspath)
        digest = hashlib.md5(os.fsencode(url)).hexdigest()
        filename = cast(str, os.path.join(self.directory, digest + ".json"))
        return filename

    def retrieve_metadata(self, abspath: str) -> Any:
        logging.info("MetaDataCache.retrieve_metadata: %s", abspath)

        json_filename = self._make_filename(abspath)

        try:
            with open(json_filename, "r") as fin:
                js = json.load(fin)
                return js
        except FileNotFoundError:
            return None
        except:
            logging.exception("MetaDataCache: unexpected exception:")
            return None

    def store_metadata(self, abspath: str, metadata: Dict) -> None:
        logging.info("MetaDataCache.store_metadata: %s", abspath)

        json_filename = self._make_filename(abspath)

        with open(json_filename, "w") as fout:
            json.dump(metadata, fout)

    def clear_metadata(self, abspath: str):
        logging.info("MetaDataCache.clear_metadata: %s", abspath)

        json_filename = self._make_filename(abspath)
        if os.path.exists(json_filename):
            os.remove(json_filename)


# EOF #