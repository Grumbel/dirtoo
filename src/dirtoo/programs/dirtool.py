# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2014 Ingo Ruhnke <grumbel@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


from typing import Tuple, Sequence, Optional

import os
import sys
import argparse
import functools
import shutil
import filecmp
from itertools import chain


# Ownership, permissions are part of the file data, not directory entry
# File structure:
# MD5SUM uid gid atime mtime ctime btime


def get_hierachy(filename: str) -> Sequence[str]:
    p = None
    np = os.path.dirname(filename)
    lst = []
    while np and p != np:
        p = np
        lst.append(np)
        np = os.path.dirname(np)
    return lst


def get_directory_list(files: Sequence[str]) -> Sequence[str]:
    """
    Returns the directory structure that holds files
    """
    return sorted(list(set(chain.from_iterable([get_hierachy(f) for f in files]))))


def move_files(sourcedir: str, targetdir: str, files: Sequence[str], ignore_case: bool) -> None:
    """
    Moves files from sourcedir to targetdir while preserving their path structure, similar to 'rsync -R'
    """

    print("move_files(%s, %s, ...)" % (sourcedir, targetdir))

    # TODO: implement this
    assert not ignore_case, "--ignore-case is not implemented for extract-diff"

    assert os.path.isdir(sourcedir)
    assert not os.path.exists(targetdir) or os.path.isdir(targetdir)

    # Create directory hierachy
    dirs = get_directory_list(files)

    if not os.path.exists(targetdir):
        os.mkdir(targetdir)

    for d in dirs:
        path = os.path.join(targetdir, d)
        print("creating %s" % path)
        if not os.path.exists(path):
            os.mkdir(path)
        elif os.path.isdir(path):
            pass
        else:
            raise RuntimeError("%s: error: path already exist, but isn't a directory" % path)

    # Move the files over
    for f in files:
        source_file = os.path.join(sourcedir, f)
        target_file = os.path.join(targetdir, f)
        print("moving %s to %s" % (source_file, target_file))
        shutil.move(source_file, target_file)


class FileDataId:

    def __init__(self, size: int, md5sum: str) -> None:
        self.size = size
        self.md5sum = md5sum

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, FileDataId):
            return False

        return (self.size == other.size and
                self.md5sum == other.md5sum)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, FileDataId):
            return False

        return (self.size, self.md5sum) < (other.size, other.md5sum)


class FileObject:

    def __init__(self) -> None:
        self.mtime = 0
        self.ctime = 0
        self.birth = 0
        self.uid = 0
        self.gid = 0
        self.filename = ""


@functools.total_ordering
class FileInfo:

    def __init__(self, md5sum: Optional[str], filename: str) -> None:
        self.md5sum = md5sum
        self.filename = os.path.normpath(filename)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, FileInfo):
            return False

        # return (self.md5sum, self.filename) == (other.md5sum, other.filename)
        return self.filename == other.filename

    def __lt__(self, other: 'FileInfo') -> bool:
        # return (self.md5sum, self.filename) < (other.md5sum, other.filename)
        return self.filename < other.filename

    def __hash__(self) -> int:
        return hash(self.filename)

    def __repr__(self) -> str:
        return "FileInfo(%s, %s)" % (repr(self.md5sum), repr(self.filename))

    def __str__(self) -> str:
        return self.filename


def fileinfo_from_path(path: str) -> Sequence[FileInfo]:
    if os.path.isdir(path):
        return fileinfo_from_directory(path)
    elif os.path.isfile(path):
        return fileinfo_from_md5sums(path)
    else:
        raise RuntimeError("%s: error: unknown file type" % path)


def fileinfo_from_md5sums(filename: str) -> Sequence[FileInfo]:
    with open(filename, "r") as fin:
        return [FileInfo(*e.split(None, 1)) for e in fin.read().splitlines()]


def fileinfo_from_directory(directory: str) -> Sequence[FileInfo]:
    lst: list[FileInfo] = []
    for path, dirs, files in os.walk(directory):
        for fname in files:
            lst.append(FileInfo(None, os.path.relpath(os.path.join(path, fname), directory)))
    return lst


def compare_directories(finfo1: Sequence[FileInfo],
                        finfo2: Sequence[FileInfo],
                        ignore_case: bool = False) -> Tuple[list[FileInfo], list[FileInfo], list[FileInfo]]:
    if ignore_case:
        finfo1_set = {FileInfo(f.md5sum, f.filename.lower()) for f in finfo1}
        finfo2_set = {FileInfo(f.md5sum, f.filename.lower()) for f in finfo2}
    else:
        finfo1_set = set(finfo1)
        finfo2_set = set(finfo2)

    return (
        sorted(finfo1_set.difference(finfo2_set)),  # removals
        sorted(finfo2_set.difference(finfo1_set)),  # additions
        []  # changes
    )


def compare_command(path1: str, path2: str, ignore_case: bool) -> None:
    finfo1 = fileinfo_from_path(path1)
    finfo2 = fileinfo_from_path(path2)

    removals, additions, changes = compare_directories(finfo1, finfo2, ignore_case)

    for f in removals:
        print("-%s" % f)

    for f in additions:
        print("+%s" % f)

    for f in changes:
        print("~%s" % f)


def extract_diff_command(path1: str, path2: str, target: str, dry_run: bool, ignore_case: bool) -> None:
    """Run a diff between path1 and path2 and move all additions to target"""

    if not os.path.isdir(path2):
        raise RuntimeError("%s: error must be a directory" % path2)

    if os.path.exists(target) and not os.path.isdir(target):
        raise RuntimeError("%s: error must be a directory" % target)

    finfo1 = fileinfo_from_path(path1)
    finfo2 = fileinfo_from_path(path2)

    removals, additions, changes = compare_directories(finfo1, finfo2, ignore_case)

    for fname in additions:
        print(fname)

    files = [f.filename for f in additions]

    if dry_run:
        for f in files:
            source_file = os.path.join(path2, f)
            target_file = os.path.join(target, f)
            print("moving %s to %s" % (source_file, target_file))
    else:
        move_files(path2, target, files, ignore_case)


def merge_command(source: str, target: str, dry_run: bool, force: bool) -> None:
    if not os.path.isdir(source):
        raise RuntimeError("%s: must be a directory" % source)
    if not os.path.isdir(target):
        raise RuntimeError("%s: must be a directory" % target)

    dir_lst = []
    file_lst = []
    for path, dirs, files in os.walk(source):
        for fname in dirs:
            dir_lst.append(os.path.relpath(os.path.join(path, fname), source))
        for fname in files:
            file_lst.append(os.path.relpath(os.path.join(path, fname), source))

    # build directory hierachy
    for path in dir_lst:
        target_dir = os.path.join(target, path)
        if os.path.exists(target_dir):
            if not os.path.isdir(target_dir):
                raise RuntimeError("%s: error: already exist and is not a directory" % target_dir)
        else:
            print("mkdir %s" % target_dir)
            if not dry_run:
                os.mkdir(target_dir)

    # move files
    for path in file_lst:
        source_file = os.path.join(source, path)
        target_file = os.path.join(target, path)

        if not force and os.path.exists(target_file):
            if filecmp.cmp(source_file, target_file, shallow=False):
                print("%s: removing, already exist in target" % source_file)
                if not dry_run:
                    os.remove(source_file)
            else:
                print("%s: file already exist, not touching it" % target_file)
        else:
            print("moving %s to %s" % (source_file, target_file))
            if not dry_run:
                shutil.move(source_file, target_file)

    # cleanup old hierachy
    for path in reversed(dir_lst):
        source_dir = os.path.join(source, path)
        print("rmdir %s" % source_dir)
        if not dry_run:
            os.rmdir(source_dir)
    if not dry_run:
        os.rmdir(source)


def main(argv: Sequence[str]) -> int:
    parser = argparse.ArgumentParser(description='dirtoo')
    parser.add_argument('COMMAND', action='store', type=str,
                        help='diff, extract-diff, merge')
    parser.add_argument('FILE1', action='store', type=str,
                        help='A directory')
    parser.add_argument('FILE2', action='store', type=str,
                        help='Another directory')
    parser.add_argument('-c', '--checksum', action='store_true',
                        help="Use checksum for comparism")
    parser.add_argument('-t', '--target', metavar="DIR", action='store',
                        help="Target directory for extract")
    parser.add_argument('-f', '--force', action='store_true',
                        help="Overwrite files in target directory")
    parser.add_argument('-n', '--dry-run', action='store_true',
                        help="don't act, just show actions")
    parser.add_argument('-i', '--ignore-case', action='store_true',
                        help="ignore case differences in filenames")
    args = parser.parse_args(argv[1:])

    if args.COMMAND == "diff":
        compare_command(args.FILE1, args.FILE2, args.ignore_case)
    elif args.COMMAND == "extract-diff":
        extract_diff_command(args.FILE1, args.FILE2, args.target, args.dry_run, args.ignore_case)
    elif args.COMMAND == "merge":
        merge_command(args.FILE1, args.FILE2, args.dry_run, args.force)
    else:
        raise RuntimeError("unknown command: %s" % args.COMMAND)

    return 0


def main_entrypoint() -> None:
    main(sys.argv)


# EOF #
