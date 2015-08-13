dirtool
=======

Goals of dirtool:

* dump file system hierachy information to a simple text based file format (.json)
* allow interrupt and resume of costly operations (i.e. md5sum calculations)
* keep track of mtime, size, etc. to not recalculate md5sums sums on each traversal
* allow differences and queries on the hierachy
* ways to handle prefix
* dirhier to md5sum conversion


move.py
-------

A tool to move files and directories around the filesytem, similar to
`mv`.

Unlike the `mv` command, `move.py` will not fail when a directory of
the same name already exists in the target location, but instead merge
those two directories together.

`move.py` is at the moment limited to moving files across a single
filesystem, it will not copy files across filesystems.


file.py
-------

`find.py` is an alternative to the classic Unix `find` tool, it
provides a simpler yet more powerful syntax, while lacking many of the
more obscure features of `find`.

The expressions used to find files are real Python code with some
convenience functions added into the mix. An expression to look for
files that are younger then 5 days and that are not text files would
look like this:

    $ find.py -f 'age() < days(5) and not name("*.txt")'

`find.py` also provides print expressions that allow to customize the
output, in this case the file modification date is printed in iso
format followed by the filename:

    $ find.py -p $'{iso():>15}  {p}\n' .git/
       2015-05-17  .git/description
       2015-05-17  .git/HEAD
       2015-08-13  .git/COMMIT_EDITMSG
       2015-05-17  .git/config
       2015-05-19  .git/ORIG_HEAD
       2015-08-13  .git/index

Python's string formating can be used to change alignment and field width.

Calling external programs in tests is possible as well:

    $ find.py -f 'stdout("file -b --mime-type (FILE)") != "text/plain"' -p $'{stdout("file -b --mime-type (FILE)"):>16}  {p}\n'
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


### TODO

* [DONE] dry-run support (-n, --dry-run)
* [DONE] support for merging directories
* conflict resolution on overwrite (i.e. rename one of the files)
* copying to multiple targets at once
* detailed progress bar/reports (`-P`, `--progress`)
* multi-threading (i.e. continue copying in the background while I handle conflict resolution in the foreground)
* no overwrite warning when source and destination files are identical

