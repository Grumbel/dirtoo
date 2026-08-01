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


from typing import TypeVar, Generic, Optional

from PyQt6.QtCore import QMutex, QWaitCondition


T = TypeVar('T')


class ReturnValue(Generic[T]):
    """Pass a return value from a slot back to the signal emitting thread.
    The ReturnValue has to be send with the signal, the receiving
    thread than calls .send() to set the value, while the calling
    thread calls .receive() to wait until the value arrives."""

    def __init__(self) -> None:
        self._mutex = QMutex()
        self._wait_condition = QWaitCondition()
        self._value: Optional[T] = None

    def receive(self) -> T:
        self._mutex.lock()
        while self._value is None:
            self._wait_condition.wait(self._mutex)
        self._mutex.unlock()

        return self._value

    def send(self, value: T) -> None:
        self._value = value
        self._wait_condition.wakeAll()


# EOF #
