#!/usr/bin/python3

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


import argparse
import re
import sys
from unidecode import unidecode
from colorama import Fore, Back, Style


def parse_args(args):
    parser = argparse.ArgumentParser(description="Highlight or transliterate non-ASCII characters")
    parser.add_argument("-c", "--color", action='store_true',
                        help="Highlight non-ASCII characters")
    parser.add_argument("-t", "--transliterate", action='store_true',
                        help="Convert Unicode characters into equivalent ASCII")
    parser.add_argument("-q", "--quiet", action='store_true',
                        help="Only output lines containing non-ASCII characters")
    return parser.parse_args(args)


def main():
    args = parse_args(sys.argv[1:])

    for line in sys.stdin.readlines():
        line_contains_unicode = bool(re.search(r'([^\x00-\x7F])', line))

        if args.quiet and not line_contains_unicode:
            pass
        else:
            output = line
            if args.color:
                output = re.sub(r'([^\x00-\x7F])',
                                Back.RED + Fore.WHITE + r'\1' + Style.RESET_ALL,
                                output)

            if args.transliterate:
                output = unidecode(output)

            sys.stdout.write(output)


# EOF #
