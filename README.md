dirtool
=======

Goals of dirtool:

* dump file system hierachy information to a simple text based file format (.json)
* allow interrupt and resume of costly operations (i.e. md5sum calculations)
* keep track of mtime, size, etc. to not recalculate md5sums sums on each traversal
* allow differences and queries on the hierachy
* ways to handle prefix
* dirhier to md5sum conversion
