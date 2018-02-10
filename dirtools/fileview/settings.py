# dirtool.py - diff tool for directories
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


from PyQt5.QtCore import QObject, QSettings


class Settings(QObject):

    def __init__(self):
        super().__init__()

    def init(self, filename):
        self.settings = QSettings(filename, QSettings.IniFormat)

    def value(self, name, default=None, type=None):
        if type is None:
            return self.settings.value(name, default)
        else:
            return self.settings.value(name, default, type)

    def set_value(self, name, value):
        return self.settings.setValue(name, value)

    def load(self):
        pass

    def save(self):
        pass


settings = Settings()


# EOF #
