#!/usr/bin/env python3

# dirtools - Python Scripts for directory stuff
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


from setuptools import setup, find_packages


setup(name='dirtools',
      version='0.1.0',
      scripts=[],
      entry_points={
          'console_scripts': ['dt-move = dirtools.cmd_move:main',
                              'dt-find = dirtools.cmd_find:main',
                              'dt-fsck = dirtools.cmd_fsck:main',
                              'dt-unidecode = dirtools.cmd_unidecode:main',
                              'dt-mediainfo = dirtools.cmd_mediainfo:main',
          ],
          'gui_scripts': []
          },
      packages=['dirtools'])


# EOF #
