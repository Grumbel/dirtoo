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

import re


LOCATION_REGEX = re.compile(r'^(file)://((?:(?:/[^/]+)*))(?://(.*)|)$')
LOCATION_PAYLOAD_REGEX = re.compile(r'^([a-z]+):(.*)')


def split_payloads(text):
    result = []

    payload_texts = text.split("//")
    for payload_text in payload_texts:
        m = LOCATION_PAYLOAD_REGEX.match(payload_text)
        if m is not None:
            result.append((m.group(1), m.group(2)))
        else:
            raise Exception("failed to split payload string: {}".format(text))

    return result


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
            abspath = m.group(2)
            payload_text = m.group(3)

            if payload_text is not None:
                payloads = split_payloads(payload_text)
            else:
                payloads = []

            return Location(protocol, abspath, payloads)

    def __init__(self, protocol, path, payloads):
        self.protocol: str = protocol
        self.path: str = path
        self.payloads: List[Tuple[str, str]] = payloads

    def has_payload(self) -> bool:
        return self.payloads != []

    def as_url(self) -> str:
        payload_text = "".join(["//{}:{}".format(prot, path)
                                for prot, path in self.payloads])
        return "{}://{}{}".format(self.protocol, self.path, payload_text)

    def __str__(self):
        return self.as_url()


# EOF #
