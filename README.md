dirtools
========

dt-fsck
-------

`dt-fsck` checks the given filenames and directories for oddities such
as incorrect UTF-8 encodings, overlong filenames and such that could
cause problems when the file is moved to different file systems.


dt-move
-------

A tool to move files and directories around the filesytem, similar to
`mv`.

Unlike the `mv` command, `dt-move` will not fail when a directory of
the same name already exists in the target location, but instead merge
those two directories together.

`dt-move` is at the moment limited to moving files across a single
filesystem, it will not copy files across filesystems.


dt-find
-------

`dt-find` is an alternative to the classic Unix `find` tool, it
provides a simpler yet more powerful syntax, while lacking many of the
more obscure features of `find`.

The expressions used to find files are real Python code with some
convenience functions added into the mix. An expression to look for
files that are younger then 5 days and that are not text files would
look like this:

    $ dt-find -f 'age() < days(5) and not name("*.txt")'

`dt-find` also provides print expressions that allow to customize the
output, in this case the file modification date is printed in iso
format followed by the filename:

    $ dt-find -p '{iso():>15}  {p}' .git/
       2015-05-17  .git/description
       2015-05-17  .git/HEAD
       2015-08-13  .git/COMMIT_EDITMSG
       2015-05-17  .git/config
       2015-05-19  .git/ORIG_HEAD
       2015-08-13  .git/index

Python's string formating can be used to change alignment and field width.

Calling external programs in tests is possible as well:

    $ dt-find -f 'stdout("file -b --mime-type (FILE)") != "text/plain"' -p '{stdout("file -b --mime-type (FILE)"):>16}  {p}'
       text/x-python  ./dirtool.py~
       text/x-python  ./dirhier.py~
       text/x-python  ./dirmove.py~
     text/x-makefile  ./Makefile~
     text/x-makefile  ./Makefile
       text/x-python  ./move.py~
       text/x-python  ./dirtool.py
       text/x-python  ./dirhier.py
       text/x-python  ./move.py
       text/x-python  ./find.py~
       text/x-python  ./find.py

The above call finds all files that are not `text/plain` and print the
filename along with the mime-type.

The output can also be sorted, using via Python expressions. The value
of the expression is used as key for the sorting:

    $ dt-find -s 'size()'


dt-archiveinfo
--------------

Provides a summary of the content of the archive.


dt-guessarchivename
-------------------

A simple script that tries to guess a good filename for an archive file
based on the content of the archive.

    usage: guessarchivename [-h] [-t THRESHOLD] [-E] [-M MIN_LENGTH] [-m] [-i]
                            [-b] [-s] [-p] [-v]
                            FILE [FILE ...]

    Find common prefix from archive content

    positional arguments:
      FILE                  Archive file to search

    optional arguments:
      -h, --help            show this help message and exit
      -t THRESHOLD, --threshold THRESHOLD
                            Threshold for when a character is considered common
                            (default: 0.8)
      -E, --ignore-ext      Don't include file extension in the result
      -M MIN_LENGTH, --min-length MIN_LENGTH
                            Minimum result length that is considered valid
      -m, --move            Rename files to the suggested names
      -i, --interactive     Ask before renaming each file
      -b, --basename        Use basename only, not the full path for guessing
      -s, --strip           Strip unimportant characters from the resulting string
      -p, --preserve        Preserves the original filename by prepending it to
                            the output
      -v, --verbose         Be more verbose

