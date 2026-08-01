#!/usr/bin/env python3.7


# from __future__ import annotations
#
# Only Python3.7 supports it, but Ubuntu 18.04 still uses 3.6 by
# default. mypy 0.580 can't handle it either. So instead quote types
# and turn them into strings.


class Foo:

    def __init__(self, bar: 'Bar') -> None:
        pass


class Bar:

    def __init__(self, foo: Foo) -> None:
        pass


# EOF #
