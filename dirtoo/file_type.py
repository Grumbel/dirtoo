# dirtoo - File and directory manipulation tools for Python
# Copyright (C) 2018 Ingo Ruhnke <grumbel@gmail.com>
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


VIDEO_EXT = ['wmv', 'mp4', 'mpg', 'mpeg', 'm2v', 'avi', 'flv', 'mkv', 'wmv',
             'mov', 'webm', 'f4v', 'flv', 'divx', 'ogv', 'vob', '3gp', '3g2',
             'qt', 'asf', 'amv', 'm4v']

VIDEO_REGEX = r"\.({})$".format("|".join(VIDEO_EXT))


IMAGE_EXT = ['jpg', 'jpeg', 'gif', 'png', 'tif', 'tiff', 'webp', 'bmp', 'xcf']

IMAGE_REGEX = r"\.({})$".format("|".join(IMAGE_EXT))


ARCHIVE_EXT = ['zip', 'rar', 'tar', 'gz', 'xz', 'bz2', 'ar', '7z']

ARCHIVE_REGEX = r"\.({})$".format("|".join(ARCHIVE_EXT))


# EOF #
