class Bar:

    def __init__(self, foo: 'Foo') -> None:
        self.foo = foo


# While a little ugly, this seems to be the only way to make circular
# imports work with python3, mypy and flake8 combined:
from foo import Foo  # noqa: F401


# EOF #
