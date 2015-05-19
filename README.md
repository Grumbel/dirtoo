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


### TODO

* [DONE] dry-run support (-n, --dry-run)
* [DONE] support for merging directories
* conflict resolution on overwrite (i.e. rename one of the files)
* copying to multiple targets at once
* detailed progress bar/reports (`-P`, `--progress`)
* multi-threading (i.e. continue copying in the background while I handle conflict resolution in the foreground)
* no overwrite warning when source and destination files are identical

