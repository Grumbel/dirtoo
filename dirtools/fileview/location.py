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


from typing import List, NamedTuple, Optional

import logging
import os
import re
from functools import total_ordering

logger = logging.getLogger(__name__)


LOCATION_REGEX = re.compile(r'^([a-z]+)://(.*)$')


# URL: {PROTOCOL}://{PAYLOAD}//{PLUGIN}:{PLUGIN_PAYLOAD}
# Example:
# http://www.example.com/foobar.jpg
# file:///www.example.com/foobar.rar//rar:Filename.jpg

class Payload(NamedTuple):
    protocol: str
    path: str


@total_ordering
class Location:

    @staticmethod
    def join(location: 'Location', path: str) -> 'Location':
        if len(location._payloads) == 0:
            result = location.copy()
            result._path = os.path.join(result._path, path)
            return result
        else:
            result = location.copy()
            result._payloads[-1] = Payload(result._payloads[-1].protocol,
                                           os.path.join(result._payloads[-1].path, path))
            return result

    @staticmethod
    def from_path(path) -> 'Location':
        return Location.from_url("file://" + os.path.abspath(path))

    @staticmethod
    def from_url(url) -> 'Location':
        m = LOCATION_REGEX.match(url)
        if m is None:
            raise Exception("Location.from_string: failed to decode: {}".format(url))
        else:
            protocol = m.group(1)
            rest = m.group(2)
            abspath, *payload_specs = rest.split("//")

            abspath = os.path.normpath(abspath)

            payloads: List[Payload] = []
            for payload_spec in payload_specs:
                payload = payload_spec.split(":", 1)
                if len(payload) == 1:
                    payloads.append(Payload(payload[0], ""))
                else:
                    payloads.append(Payload(payload[0], payload[1]))

            return Location(protocol, abspath, payloads)

    def __init__(self, protocol: str, path: str, payloads: List[Payload]) -> None:
        assert os.path.isabs(path)

        self._protocol: str = protocol
        self._path: str = path
        self._payloads: List[Payload] = payloads

    def has_payload(self) -> bool:
        return self._payloads != []

    def parent(self) -> 'Location':
        """The parent directory. Archives are threated like directories as well."""

        if self._payloads == [] or (len(self._payloads) == 1 and self._payloads[0].path == ""):
            path = os.path.dirname(self._path)
            return Location(self._protocol, path, [])
        else:
            path = os.path.dirname(self._payloads[-1].path)
            if path == self._payloads[-1].path:  # path is ""
                payloads = self._payloads[:-1]
                payloads[-1] = Payload(payloads[-1].protocol,
                                       os.path.dirname(payloads[-1].path))
                return Location(self._protocol, self._path, payloads)
            else:
                payloads = self._payloads[:-1] + [Payload(self._payloads[-1].protocol, path)]
                return Location(self._protocol, self._path, payloads)

    def origin(self) -> Optional['Location']:
        """The location that is 'housing' self. For a while inside an archive,
        this would return the location of the archive itself."""

        if self._payloads == []:
            return None
        else:
            location = self.copy()
            location._payloads.pop()
            return location

    def as_url(self) -> str:
        payload_text = "".join(["//{}{}".format(prot, (":" + path) if path else "")
                                for prot, path in self._payloads])
        return "{}://{}{}".format(self._protocol, self._path, payload_text)

    def as_path(self) -> str:
        """Like .as_url() but without the protocol part. Only use this for
        display purpose, as information is lost."""
        payload_text = "".join(["//{}{}".format(prot, (":" + path) if path else "")
                                for prot, path in self._payloads])
        return "{}://{}{}".format(self._protocol, self._path, payload_text)

    def exists(self) -> bool:
        if self.has_payload():
            if len(self._payloads) != 1:
                logger.error("Location.exists: not implemented for multiple payloads")
                return False
            else:
                return os.path.exists(self._path)
        else:
            return os.path.exists(self._path)

    def copy(self) -> 'Location':
        return Location(self._protocol, self._path, list(self._payloads))

    def get_path(self) -> str:
        assert not self.has_payload()
        return self._path

    def __eq__(self, other):
        return (self._protocol, self._path, self._payloads) == (other._protocol, other._path, other._payloads)

    def __ne__(self, other):
        return not (self == other)

    def __lt__(self, other):
        return (self._protocol, self._path, self._payloads) < (other._protocol, other._path, other._payloads)

    def __hash__(self):
        # import timeit
        # timeit.timeit('hash(a)',
        #               "from dirtools.fileview.location import Location; "
        #               "a = Location.from_url('file:///home/foo/foo.rar//archive:bar.jpg');")
        # as_url: 3.0
        # tuple (_payloads as list): 0.9
        # tuple (_payloads as tuple): 0.6
        # self.path: 0.45
        return hash((self._protocol, self._path, tuple(self._payloads)))
        # return hash(self.as_url())
        # return hash(self._path)

    def __str__(self):
        return self.as_url()


# EOF #
