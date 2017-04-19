# dirtool.py - diff tool for directories
# Copyright (C) 2015 Ingo Ruhnke <grumbel@gmail.com>
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


import sys
import argparse


from dirtools.find.action import MultiAction, PrinterAction, ExecAction, ExprSorterAction
from dirtools.find.filter import ExprFilter, NoFilter
from dirtools.find.util import find_files


def parse_args(args):
    parser = argparse.ArgumentParser(description="Find files")

    parser.add_argument("DIRECTORY", nargs='*')
    parser.add_argument("-0", "--null", action="store_true",
                        help="Print \0 delimitered file list")
    parser.add_argument("-f", "--filter", metavar="EXPR", type=str,
                        help="Filter filename through EXPR")
    parser.add_argument("-s", "--sort", metavar="EXPR", type=str,
                        help="Sort filename by EXPR")
    parser.add_argument("-R", "--reverse", default=False, action='store_true',
                        help="Reverse sort order")
    parser.add_argument("-r", "--recursive", action='store_true',
                        help="Recursize into the directory tree")
    parser.add_argument("-l", "--list", action='store_true',
                        help="List files verbosely")
    parser.add_argument("-p", "--println", metavar="FMT",
                        help="List files with the given format string (with newline)")
    parser.add_argument("-P", "--print", metavar="FMT",
                        help="List files with the given format string (without newline)")
    parser.add_argument("-q", "--quiet", action='store_true',
                        help="Be quiet")
    parser.add_argument("--exec", metavar="EXEC",
                        help="Execute EXEC")

    return parser.parse_args(args)


def create_action(args):
    action = MultiAction()

    if args.quiet:
        pass
    elif args.null:
        action.add(PrinterAction("{fullpath()}\0"))
    elif args.list:
        action.add(PrinterAction("{modehr()}  {owner()}  {group()}  {sizehr():>8}  {iso()} {time()}  {fullpath()}\n",
                                 finisher=True))
    elif args.println:
        action.add(PrinterAction(args.println + "\n"))
    elif args.print:
        action.add(PrinterAction(args.print))
    else:
        action.add(PrinterAction("{fullpath()}\n"))

    if args.exec:
        action.add(ExecAction(args.exec))

    return action


def create_filter(args):
    if args.filter:
        return ExprFilter(args.filter)
    else:
        return NoFilter()


def create_sorter_wrapper(args, find_action):
    if args.sort is None and not args.reverse:
        return find_action
    else:
        return ExprSorterAction(args.sort, args.reverse, find_action)


def main():
    args = parse_args(sys.argv[1:])

    directories = args.DIRECTORY or ['.']

    find_action = create_action(args)
    find_filter = create_filter(args)

    find_action = create_sorter_wrapper(args, find_action)

    for d in directories:
        find_files(d, args.recursive, find_filter, find_action)

    find_action.finish()


# EOF #
