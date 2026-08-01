# The 'if False' allows to import types that would cause a runtime
# circular dependency, but are needed for checking of types that have
# been added via string'ified forward references.
if False:
    from foo import Foo  # noqa: F401


class Bar:

    def __init__(self, foo: 'Foo') -> None:
        self.foo = foo


# EOF #
