# The 'if False' allows to import types that would cause a runtime
# circular dependency, but are needed for checking of types that have
# been added via string'ified forward references.
if False:
    from bar import Bar  # noqa: F401


class Foo:

    def __init__(self, bar: 'Bar') -> None:
        self.bar = bar


# EOF #
