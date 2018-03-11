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


from typing import List, Optional

import sys
import argparse

from dirtools.find.action import Action, MultiAction, PrinterAction, ExecAction, ExprSorterAction
from dirtools.find.filter import ExprFilter, SimpleFilter, NoFilter
from dirtools.find.util import find_files


def parse_args(args: List[str], simple) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Find files")

    if simple:
        parser.add_argument("QUERY", nargs='*')
    else:
        parser.add_argument("DIRECTORY", nargs='*')

    trav_grp = parser.add_argument_group("Traversial Options")

    if simple:
        trav_grp.add_argument("-d", "--directory", metavar="DIR", type=str, action='append',
                              help="Directories to search")

    trav_grp.add_argument("--depth", action='store_true', default=False,
                          help="Process directory content before the directory itself")
    trav_grp.add_argument("-D", "--maxdepth", metavar="INT", type=int, default=None,
                          help="Maximum recursion depth")

    print_grp = parser.add_argument_group("Print Options")
    print_grp.add_argument("-0", "--null", action="store_true",
                           help="Print \0 delimitered file list")
    print_grp.add_argument("-l", "--list", action='store_true',
                           help="List files verbosely")
    print_grp.add_argument("-p", "--println", metavar="FMT",
                           help="List files with the given format string (with newline)")
    print_grp.add_argument("-P", "--print", metavar="FMT",
                           help="List files with the given format string (without newline)")
    print_grp.add_argument("-q", "--quiet", action='store_true',
                           help="Be quiet")

    sort_grp = parser.add_argument_group("Sort Options")
    sort_grp.add_argument("-s", "--sort", metavar="EXPR", type=str,
                          help="Sort filename by EXPR")
    sort_grp.add_argument("-R", "--reverse", default=False, action='store_true',
                          help="Reverse sort order")

    filter_grp = parser.add_argument_group("Filter Options")
    filter_grp.add_argument("-f", "--filter", metavar="EXPR", type=str,
                            help="Filter filename through EXPR")

    action_grp = parser.add_argument_group("Action Options")
    action_grp.add_argument("--exec", metavar="CMD",
                            help="Execute CMD")

    return parser.parse_args(args)


def create_action(args: argparse.Namespace) -> Action:
    action = MultiAction()

    if args.quiet:
        pass
    elif args.null:
        action.add(PrinterAction("{fullpath()}\0"))
    elif args.list:
        action.add(PrinterAction("{modehr()}  {owner()}  {group()}  {sizehr():>9}  {iso()} {time()}  {fullpath()}\n",
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


def create_filter(filter_text: Optional[str]):
    if filter_text:
        return ExprFilter(filter_text)
    else:
        return NoFilter()


def create_simple_filter(filter_text: Optional[str]):
    if filter_text:
        return ExprFilter(filter_text)
    else:
        return NoFilter()


def create_sorter_wrapper(args: argparse.Namespace, find_action):
    if args.sort is None and not args.reverse:
        return find_action
    else:
        return ExprSorterAction(args.sort, args.reverse, find_action)


def main(argv, simple):
    args = parse_args(argv[1:], simple)

    find_action = create_action(args)
    find_action = create_sorter_wrapper(args, find_action)

    if simple:
        find_filter = SimpleFilter.from_string(" ".join(args.QUERY))
        directories = args.directory or ["."]
    else:
        find_filter = create_filter(args.filter)
        directories = args.DIRECTORY or ['.']

    for d in directories:
        find_files(d, find_filter, find_action, topdown=not args.depth, maxdepth=args.maxdepth)

    find_action.finish()


def search_entrypoint():
    main(sys.argv, simple=True)


def find_entrypoint():
    main(sys.argv, simple=False)


# EOF #
