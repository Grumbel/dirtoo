class Foo:

    def __init__(self, bar: 'Bar') -> None:
        self.bar = bar


# While a little ugly, this seems to be the only way to make circular
# imports work with python3, mypy and flake8 combined:
from bar import Bar  # noqa: F401


# EOF #
