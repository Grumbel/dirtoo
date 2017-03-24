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
from colorama import Fore, Back, Style


def parse_args(args):
    parser = argparse.ArgumentParser(description="Highlight non-ascii characters")
    return parser.parse_args(args)

def main():
    args = parse_args(sys.argv[1:])

    for line in sys.stdin.readlines():
        line_colored = re.sub(r'([^\x00-\x7F])',
                              Back.RED + Fore.WHITE + r'\1' + Style.RESET_ALL,
                              line)
        sys.stdout.write(line_colored)


# EOF #
