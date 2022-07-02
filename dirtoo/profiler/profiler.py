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


from typing import Any, Optional, Type, Callable, Union
from types import TracebackType

import time


_profiler_active = False


def activate_profiler(active: bool) -> None:
    print("PROFILER", active)
    global _profiler_active
    _profiler_active = active


class RealProfiler:

    def __init__(self, name: str) -> None:
        self.name = name

    def __enter__(self) -> None:
        self.start_time = time.time()

    def __exit__(self,  # pylint: disable=useless-return
                 exc_type: Optional[Type[BaseException]],
                 exc_value: Optional[BaseException],
                 traceback: Optional[TracebackType]) -> Optional[bool]:
        self.stop_time = time.time()
        print("executing {} took {:3.0f}msec".format(
            self.name, (self.stop_time - self.start_time) * 1000))
        return None


class DummyProfiler:

    def __init__(self) -> None:
        pass

    def __enter__(self) -> None:
        pass

    def __exit__(self,  # pylint: disable=useless-return
                 exc_type: Optional[Type[BaseException]],
                 exc_value: Optional[BaseException],
                 traceback: Optional[TracebackType]) -> Optional[bool]:
        return None


def Profiler(name: str) -> Union[RealProfiler, DummyProfiler]:
    global _profiler_active
    if _profiler_active:
        return RealProfiler(name)
    else:
        return DummyProfiler()


def profile(func: Callable[[Any], Any]) -> Callable[[Any], Any]:
    def wrap(*args: Any, **kwargs: Any) -> Any:
        if _profiler_active:
            with RealProfiler(func.__qualname__):
                result = func(*args, **kwargs)
                return result
        else:
            return func(*args, **kwargs)

    return wrap


# EOF #
