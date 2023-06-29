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


from typing import (cast, Dict, Any, TypeVar, Generic, Iterable,
                    Optional, Callable, Sized, Iterator)


KT = TypeVar('KT')
VT = TypeVar('VT')


class ListDict(Generic[KT, VT], Sized, Iterable[VT]):
    """A data structure that preserves ordering as well as allow fast
    replacement of objects. 'None' values can't be stored as they are
    used internally to allow fast removal."""

    def __init__(self,
                 key_func: Callable[[VT], KT],
                 iterable: Optional[Iterable[VT]] = None) -> None:
        self._key_func = key_func

        self._list: list[Optional[VT]]
        if iterable is None:
            self._list = []
            self._key2idx: Dict[KT, int] = {}
        else:
            self._list = list(iterable)
            self._rebuild_index()

    def _rebuild_index(self) -> None:
        self._key2idx = {self._key_func(x): idx for idx, x in enumerate(self._list) if x is not None}

    def __iter__(self) -> Iterator[VT]:
        return (x for x in self._list if x is not None)

    def replace(self, key: KT, value: VT) -> None:
        idx = self._key2idx[key]
        self._list[idx] = value

    def append(self, value: VT) -> None:
        idx = self._key_func(value)
        self._key2idx[idx] = len(self._list)
        self._list.append(value)

    def get(self, key: KT, default: Optional[Any] = None) -> Optional[VT]:
        idx = self._key2idx.get(key, default)
        if idx is not None:
            return self._list[idx]
        else:
            return None

    def remove(self, key: KT) -> None:
        idx = self._key2idx[key]
        self._list[idx] = None
        del self._key2idx[key]

    def sort(self, key: Callable[[VT], Any], reverse: bool = False) -> None:
        self._list = [x for x in self._list if x is not None]
        self._list.sort(key=cast(Callable[[Optional[VT]], Any], key),
                        reverse=reverse)
        self._rebuild_index()

    def clear(self) -> None:
        self._list.clear()
        self._key2idx.clear()

    def __contains__(self, key: KT) -> bool:
        return key in self._key2idx

    def __len__(self) -> int:
        return len(self._key2idx)

    def __str__(self) -> str:
        return str([x for x in self._list if x is not None])

    def __repr__(self) -> str:
        return repr([x for x in self._list if x is not None])


# EOF #
