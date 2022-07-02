# dirtoo - File and directory manipulation tools for Python
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


from typing import Tuple, Union, Sequence

import re


NUMERIC_SORT_RX = re.compile(r'(\d+)')


def numeric_sort_key(text: str) -> Tuple[Union[int, str], ...]:
    # For numbers this will return ('', 999, ''), the empty strings
    # must not be filtered out of the tuple, as they ensure that 'int'
    # isn't compared to a 'str'. With the empty strings in place all
    # odd positions are 'str' and all even ones are 'int'.

    return tuple(int(sub) if sub.isdigit() else sub
                 for sub in NUMERIC_SORT_RX.split(text))


def numeric_sorted(lst: Sequence[str]) -> Sequence[str]:
    return sorted(lst, key=numeric_sort_key)


# EOF #
