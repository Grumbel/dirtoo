#!/usr/bin/env python3

# dirtoo - File and directory manipulation tools for Python
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


setup(name='dirtoo',
      version='0.1.0',
      scripts=[],
      entry_points={
          'console_scripts': [
              'dt-archive-extractor = dirtoo.cmd_archive_extractor:main_entrypoint',
              'dt-swap = dirtoo.cmd_swap:main_entrypoint',
              'dt-move = dirtoo.cmd_move:move_main_entrypoint',
              'dt-copy = dirtoo.cmd_move:copy_main_entrypoint',
              'dt-chomp = dirtoo.cmd_chomp:main_entrypoint',
              'dt-expr = dirtoo.cmd_expr:main_entrypoint',
              'dt-find = dirtoo.cmd_find:find_entrypoint',
              'dt-fuzzy = dirtoo.cmd_fuzzy:main_entrypoint',
              'dt-glob = dirtoo.cmd_glob:main_entrypoint',
              'dt-search = dirtoo.cmd_find:search_entrypoint',
              'dt-mkevil = dirtoo.cmd_mkevil:main_entrypoint',
              'dt-mktest = dirtoo.cmd_mktest:main_entrypoint',
              'dt-fsck = dirtoo.cmd_fsck:main',
              'dt-dirtool = dirtoo.cmd_dirtool:main_entrypoint',
              'dt-icon = dirtoo.cmd_icon:main_entrypoint',
              'dt-mime = dirtoo.cmd_mime:main_entrypoint',
              'dt-desktop = dirtoo.cmd_desktop:main_entrypoint',
              'dt-unidecode = dirtoo.cmd_unidecode:main',
              'dt-mediainfo = dirtoo.cmd_mediainfo:main',
              'dt-fileview = dirtoo.cmd_fileview:main_entrypoint',
              'dt-archiveinfo = dirtoo.cmd_archiveinfo:main_entrypoint',
              'dt-guessarchivename = dirtoo.cmd_guessarchivename:main_entrypoint',
              'dt-thumbnailer = dirtoo.cmd_thumbnailer:main_entrypoint',
              'dt-metadata = dirtoo.cmd_metadata:main_entrypoint',
              'dt-qtthumbnailer = dirtoo.cmd_qtthumbnailer:main_entrypoint',
              'dt-guitest = dirtoo.cmd_guitest:main_entrypoint',
              'dt-sleep = dirtoo.cmd_sleep:main_entrypoint',
              'dt-rmdir = dirtoo.cmd_rmdir:main_entrypoint',
              'dt-shuffle = dirtoo.cmd_shuffle:main_entrypoint',
          ],
          'gui_scripts': []
          },
      install_requires=[
          'bytefmt',
          'ngram',
          'pyparsing',
          'pyxdg',
          # 'PyQt5',
          # 'MediaInfoDLL3',
          'PyPDF2',
          'libarchive-c',
          'inotify_simple',
          'sortedcontainers',
          'numpy',
          'scipy',
      ],
      packages=['dirtoo', 'dirtoo.fileview', 'dirtoo.find', 'dirtoo.expr'],
      include_package_data=True,
      package_data={'dirtoo': ['dirtoo/fileview/fileview.svg',
                                 'dirtoo/fileview/icons/*.gif',
                                 'dirtoo/fileview/icons/*.png']},
      data_files=[
          ('share/icons/hicolor/scalable/apps', ['dirtoo/fileview/dt-fileview.svg']),
          ('share/applications', ['dt-fileview.desktop'])
      ],
)


# EOF #
