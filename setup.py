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
          'console_scripts': [
              'dt-archive-extractor = dirtools.cmd_archive_extractor:main_entrypoint',
              'dt-move = dirtools.cmd_move:move_main_entrypoint',
              'dt-copy = dirtools.cmd_move:copy_main_entrypoint',
              'dt-chomp = dirtools.cmd_chomp:main_entrypoint',
              'dt-expr = dirtools.cmd_expr:main_entrypoint',
              'dt-find = dirtools.cmd_find:find_entrypoint',
              'dt-fuzzy = dirtools.cmd_fuzzy:main_entrypoint',
              'dt-search = dirtools.cmd_find:search_entrypoint',
              'dt-mkevil = dirtools.cmd_mkevil:main_entrypoint',
              'dt-mktest = dirtools.cmd_mktest:main_entrypoint',
              'dt-fsck = dirtools.cmd_fsck:main',
              'dt-dirtool = dirtools.cmd_dirtool:main_entrypoint',
              'dt-icon = dirtools.cmd_icon:main_entrypoint',
              'dt-mime = dirtools.cmd_mime:main_entrypoint',
              'dt-desktop = dirtools.cmd_desktop:main_entrypoint',
              'dt-unidecode = dirtools.cmd_unidecode:main',
              'dt-mediainfo = dirtools.cmd_mediainfo:main',
              'dt-fileview = dirtools.cmd_fileview:main_entrypoint',
              'dt-archiveinfo = dirtools.cmd_archiveinfo:main_entrypoint',
              'dt-guessarchivename = dirtools.cmd_guessarchivename:main_entrypoint',
              'dt-thumbnailer = dirtools.cmd_thumbnailer:main_entrypoint',
              'dt-metadata = dirtools.cmd_metadata:main_entrypoint',
              'dt-qtthumbnailer = dirtools.cmd_qtthumbnailer:main_entrypoint',
              'dt-guitest = dirtools.cmd_guitest:main_entrypoint',
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
          'sortedcollections',
          'numpy',
          'scipy'
      ],
      packages=['dirtools', 'dirtools.fileview', 'dirtools.find'],
      include_package_data=True,
      package_data={'dirtools': ['dirtools/fileview/fileview.svg',
                                 'dirtools/fileview/icons/*.gif',
                                 'dirtools/fileview/icons/*.png']},
      data_files=[
          ('share/icons/hicolor/scalable/apps', ['dirtools/fileview/dt-fileview.svg']),
          ('share/applications', ['dt-fileview.desktop'])
      ],
)


# EOF #
