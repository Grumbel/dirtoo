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


from typing import List, NamedTuple, Optional, Tuple

import urllib.parse
import logging
import os
import re
from functools import total_ordering

logger = logging.getLogger(__name__)


LOCATION_REGEX = re.compile(r'^([a-z0]+)://(.*)$', re.DOTALL)


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
    def from_search_query(path: str, query: str) -> 'Location':
        return Location("search", path, [Payload("query", query)])

    @staticmethod
    def from_path(path: str) -> 'Location':
        return Location.from_url("file://" + os.path.abspath(path))

    @staticmethod
    def from_human(path: str) -> 'Location':
        try:
            return Location.from_url(path)
        except Exception:
            if path == "":
                # Interpret the empty string as the current directory,
                # mimicing the behaviour of os.path.abspath("").
                return Location.from_url("file://" + os.getcwd())
            elif path[0] == "/":
                return Location.from_url("file://" + path)
            else:
                return Location.from_url("file://" + os.path.join(os.getcwd(), path))

    @staticmethod
    def from_url(url: str) -> 'Location':
        m = LOCATION_REGEX.match(url)
        if m is None:
            raise Exception("Location.from_url: failed to decode: {}".format(url))
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

    def __init__(self, protocol: str, abspath: str, payloads: List[Payload]) -> None:
        assert os.path.isabs(abspath)

        self._protocol: str = protocol
        self._path: str = abspath
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

    def basename(self) -> str:
        """Returns the last element in the URL."""

        if self._payloads == []:
            return os.path.basename(self._path)
        elif len(self._payloads) == 1 and self._payloads[-1].path == "":
            return "{}//{}".format(os.path.basename(self._path),
                                   self._payloads[-1].protocol)
        elif self._payloads[-1].path == "":
            return "{}//{}".format(os.path.basename(self._payloads[-2].path),
                                   self._payloads[-1].protocol)
        else:
            return "{}".format(os.path.basename(self._payloads[-1].path))

    def ancestry(self) -> List['Location']:
        """Return a list of parents of this Location as well as the Location
        itself."""

        ancestry: List['Location'] = [self]

        last_parent = self
        while True:
            parent = last_parent.parent()
            if parent == last_parent:
                break

            ancestry.append(parent)
            last_parent = parent

        return list(reversed(ancestry))

    def origin(self) -> Optional['Location']:
        """The location that is 'housing' self. For example for a Location
        pointing to inside an archive, this would return the location
        of the archive itself."""

        if self._payloads == []:
            return None
        else:
            location = self.copy()
            location._payloads.pop()
            return location

    def pure(self) -> 'Location':
        """Returns Location with the last payload stripped if the payload path
        is empty, i.e. strip the '//archive' part if it exist."""

        if self._payloads == [] or self._payloads[-1].path != "":
            return self.copy()
        else:
            location = self.copy()
            location._payloads.pop()
            return location

    def as_url(self) -> str:
        payload_text = "".join(["//{}{}".format(prot, (":" + urllib.parse.quote(path)) if path else "")
                                for prot, path in self._payloads])
        return ("{}://{}{}".format(
            self._protocol,
            urllib.parse.quote(self._path),
            payload_text))

    def as_path(self) -> str:
        """Like .as_url() but without the protocol part. Only use this for
        display purpose, as information is lost."""
        payload_text = "".join(["//{}{}".format(prot, (":" + path) if path else "")
                                for prot, path in self._payloads])
        return "{}{}".format(self._path, payload_text)

    def as_human(self) -> str:
        if self._protocol == "file":
            return self.as_path()
        else:
            return self.as_url()

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

    def protocol(self) -> str:
        return self._protocol

    def get_path(self) -> str:
        assert not self.has_payload()
        return self._path

    def has_stdio_name(self) -> bool:
        return not self.has_payload()

    def get_stdio_name(self) -> str:
        assert self.has_stdio_name()
        return self.get_path()

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Location):
            return (self._protocol, self._path, self._payloads) == (other._protocol, other._path, other._payloads)
        else:
            return False

    def __ne__(self, other: object) -> bool:
        return not (self == other)

    def __lt__(self, other: 'Location') -> bool:
        return (self._protocol, self._path, self._payloads) < (other._protocol, other._path, other._payloads)

    def __hash__(self) -> int:
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

    def __str__(self) -> str:
        return self.as_url()

    def __repr__(self) -> str:
        return "Location.from_url({!r})".format(self.as_url())

    def search_query(self) -> Tuple[str, str]:
        assert self._protocol == "search"
        assert len(self._payloads) == 1
        assert self._payloads[0].protocol == "query"

        return (self._path, self._payloads[0].path)


# EOF #
