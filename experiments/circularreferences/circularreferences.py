#!/usr/bin/env python3

class Foo:

    def __init__(self, bar: 'Bar'):
        pass

class Bar:

    def __init__(self, foo: Foo):
        pass


# EOF #
