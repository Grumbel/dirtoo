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


from typing import List, Tuple

import os
import re


LOCATION_REGEX = re.compile(r'^([a-z]+)://(.*)$')


# URL: {PROTOCOL}://{PAYLOAD}//{PLUGIN}:{PLUGIN_PAYLOAD}
# Example:
# http://www.example.com/foobar.jpg
# file:///www.example.com/foobar.rar//rar:Filename.jpg
class Location:

    @staticmethod
    def from_path(path):
        return Location.from_url("file://" + path)

    @staticmethod
    def from_url(url):
        m = LOCATION_REGEX.match(url)
        if m is None:
            raise Exception("Location.from_string: failed to decode: {}".format(url))
        else:
            protocol = m.group(1)
            rest = m.group(2)
            abspath, *rest = rest.split("//")

            abspath = os.path.normpath(abspath)

            payloads = []
            for payload_spec in rest:
                m = payload_spec.split(":", 1)
                if len(m) == 1:
                    payloads.append((m[0], None))
                else:
                    payloads.append((m[0], m[1]))

            return Location(protocol, abspath, payloads)

    def __init__(self, protocol, path, payloads):
        self.protocol: str = protocol
        self.path: str = path
        self.payloads: List[Tuple[str, str]] = payloads

    def has_payload(self) -> bool:
        return self.payloads != []

    def as_url(self) -> str:
        payload_text = "".join(["//{}{}".format(prot, (":" + path) if path else "")
                                for prot, path in self.payloads])
        return "{}://{}{}".format(self.protocol, self.path, payload_text)

    def __str__(self):
        return self.as_url()


# EOF #
