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
              'dt-archive-extractor = dirtoo.programs.archive_extractor:main_entrypoint',
              'dt-swap = dirtoo.programs.swap:main_entrypoint',
              'dt-move = dirtoo.programs.move:move_main_entrypoint',
              'dt-copy = dirtoo.programs.move:copy_main_entrypoint',
              'dt-chomp = dirtoo.programs.chomp:main_entrypoint',
              'dt-expr = dirtoo.programs.expr:main_entrypoint',
              'dt-find = dirtoo.programs.find:find_entrypoint',
              'dt-fuzzy = dirtoo.programs.fuzzy:main_entrypoint',
              'dt-glob = dirtoo.programs.glob:main_entrypoint',
              'dt-search = dirtoo.programs.find:search_entrypoint',
              'dt-mkevil = dirtoo.programs.mkevil:main_entrypoint',
              'dt-mktest = dirtoo.programs.mktest:main_entrypoint',
              'dt-fsck = dirtoo.programs.fsck:main',
              'dt-dirtool = dirtoo.programs.dirtool:main_entrypoint',
              'dt-icon = dirtoo.programs.icon:main_entrypoint',
              'dt-mime = dirtoo.programs.mime:main_entrypoint',
              'dt-desktop = dirtoo.programs.desktop:main_entrypoint',
              'dt-unidecode = dirtoo.programs.unidecode:main',
              'dt-mediainfo = dirtoo.programs.mediainfo:main',
              'dt-fileview = dirtoo.programs.fileview:main_entrypoint',
              'dt-archiveinfo = dirtoo.programs.archiveinfo:main_entrypoint',
              'dt-guessarchivename = dirtoo.programs.guessarchivename:main_entrypoint',
              'dt-thumbnailer = dirtoo.programs.thumbnailer:main_entrypoint',
              'dt-metadata = dirtoo.programs.metadata:main_entrypoint',
              'dt-qtthumbnailer = dirtoo.programs.qtthumbnailer:main_entrypoint',
              'dt-guitest = dirtoo.programs.guitest:main_entrypoint',
              'dt-sleep = dirtoo.programs.sleep:main_entrypoint',
              'dt-rmdir = dirtoo.programs.rmdir:main_entrypoint',
              'dt-shuffle = dirtoo.programs.shuffle:main_entrypoint',
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
      packages=find_packages(),
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
