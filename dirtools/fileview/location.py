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

@total_ordering
class Location:

    @staticmethod
    def join(location: 'Location', path: str) -> 'Location':
        if len(location.payloads) == 1:
            result = location.copy()
            result.payloads[0] = (result.payloads[0][0], os.path.join(result.payloads[0][1], path))
            return result
        elif len(location.payloads) == 0:
            result = location.copy()
            result.path = os.path.join(result.path, path)
            return result
        else:
            raise Exception("Location.join: not implemented for multiple payloads")

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

            payloads: List[Tuple[str, str]] = []
            for payload_spec in payload_specs:
                payload = payload_spec.split(":", 1)
                if len(payload) == 1:
                    payloads.append((payload[0], ""))
                else:
                    payloads.append((payload[0], payload[1]))

            return Location(protocol, abspath, payloads)

    def __init__(self, protocol: str, path: str, payloads: List[Tuple[str, str]]) -> None:
        assert os.path.isabs(path)

        self.protocol: str = protocol
        self.path: str = path
        self.payloads: List[Tuple[str, str]] = payloads

    def has_payload(self) -> bool:
        return self.payloads != []

    def parent(self) -> 'Location':
        if self.payloads == [] or (len(self.payloads) == 1 and self.payloads[0][1] == ""):
            path = os.path.dirname(self.path)
            return Location(self.protocol, path, [])
        else:
            path = os.path.dirname(self.payloads[-1][1])
            if path == self.payloads[-1][1]:
                return Location(self.protocol, self.path, self.payloads[:-1])
            else:
                payloads = self.payloads[:-1] + [(self.payloads[-1][0], path)]
                return Location(self.protocol, self.path, payloads)

    def as_url(self) -> str:
        payload_text = "".join(["//{}{}".format(prot, (":" + path) if path else "")
                                for prot, path in self.payloads])
        return "{}://{}{}".format(self.protocol, self.path, payload_text)

    def as_path(self) -> str:
        """Like .as_url() but without the protocol part. Only use this for
        display purpose, as information is lost."""
        payload_text = "".join(["//{}{}".format(prot, (":" + path) if path else "")
                                for prot, path in self.payloads])
        return "{}://{}{}".format(self.protocol, self.path, payload_text)

    def abspath(self):
        assert not self.has_payload()
        return self.path

    def has_stdio_name(self) -> bool:
        return not self.has_payload()

    def get_stdio_name(self) -> str:
        assert self.payloads == []
        return self.path

    def exists(self) -> bool:
        if self.has_payload():
            if len(self.payloads) != 1:
                logger.error("Location.exists: not implemented for multiple payloads")
                return False
            else:
                return os.path.exists(self.path)
        else:
            return os.path.exists(self.path)

    def copy(self) -> 'Location':
        return Location(self.protocol, self.path, list(self.payloads))

    def __eq__(self, other):
        return (self.protocol, self.path, self.payloads) == (other.protocol, other.path, other.payloads)

    def __ne__(self, other):
        return not (self == other)

    def __lt__(self, other):
        return (self.protocol, self.path, self.payloads) < (other.protocol, other.path, other.payloads)

    def __hash__(self):
        # FIXME: this could be made faster
        return hash(self.as_url())

    def __str__(self):
        return self.as_url()


# EOF #
