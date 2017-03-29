#!/usr/bin/python3

# dirtool.py - diff tool for directories
# Copyright (C) 2017 Ingo Ruhnke <grumbel@gmail.com>
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


import os
import sys
import argparse
import datetime
import signal

from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPixmap
from PyQt5.QtWidgets import (
    QWidget, QApplication, QGraphicsView, QSizePolicy,
    QGraphicsScene, QVBoxLayout,
    QLabel, QLineEdit,
    QGraphicsPixmapItem,
    QMainWindow
)

from dirtools.fileview.file_item import FileItem
from dirtools.thumbnail import make_thumbnail_filename


class ThumbView(QGraphicsView):

    def __init__(self):
        super().__init__()
        self.setAcceptDrops(True)

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

    def dragMoveEvent(self, e):
        # the default implementation will check if any item in the
        # scene accept a drop event, we don't want that, so we
        # override the function to do nothing
        pass

    def dragEnterEvent(self, e):
        print(e.mimeData().formats())
        if e.mimeData().hasFormat("text/uri-list"):
            e.accept()
        else:
            e.ignore()

    def dropEvent(self, e):
        print("drag leve")
        print(e.mimeData().urls())
        # [PyQt5.QtCore.QUrl('file:///home/ingo/projects/dirtool/trunk/setup.py')]

    def add_files(self, files):
        self.files = files
        self.thumbnails = []

        for filename in self.files:
            thumbnail_filename = make_thumbnail_filename(filename)
            if thumbnail_filename:
                thumb = QGraphicsPixmapItem(QPixmap(thumbnail_filename))
                self.scene.addItem(thumb)
                self.thumbnails.append(thumb)
                thumb.setToolTip(filename)
                thumb.show()

        self.layout_thumbnails()

    def layout_thumbnails(self):
        x = 0
        y = 0
        max_height = 0
        for thumb in self.thumbnails:
            thumb.setPos(x, y)
            x += thumb.boundingRect().width()
            max_height = max(max_height, thumb.boundingRect().height())
            if x > 1024:
                y += max_height
                x = 0
                max_height = 0


class DetailView(QGraphicsView):

    def __init__(self):
        super().__init__()

        self.timespace = False
        self.file_items = []

        self.scene = QGraphicsScene()
        self.setScene(self.scene)

    def add_files(self, files):
        self.files = files
        self.layout_files()

    def layout_files(self):
        self.scene.clear()
        self.file_items = []

        last_mtime = None

        files = [(os.path.getmtime(f), f) for f in self.files]
        files = sorted(files)

        y = 0
        for idx, (mtime, filename) in enumerate(files):

            if self.timespace:
                if last_mtime is not None:
                    diff = mtime - last_mtime
                    # print(diff)
                    y += min(100, diff / 1000)
                    # FIXME: add line of varing color to signal distance

            last_mtime = mtime

            text = self.scene.addText(datetime.datetime.fromtimestamp(mtime).strftime("%F %T"))
            text.setPos(20, y)

            text = FileItem(filename)
            text.filename = filename
            self.scene.addItem(text)
            text.setPos(200, y)

            self.file_items.append(text)
            # print(dir(text))
            # text.mousePressEvent.connect(lambda *args, filename=filename: self.on_file_click(filename, *args))

            y += 20

        self.setSceneRect(self.scene.itemsBoundingRect())
        # print(self.scene.itemsBoundingRect())

    def toggle_timegaps(self):
        self.timespace = not self.timespace
        self.layout_files()

    # def mousePressEvent(self, ev):
    #     print("View:click")
    #     super().mousePressEvent(ev)

    def show_abspath(self):
        for item in self.file_items:
            item.show_abspath()

    def show_basename(self):
        for item in self.file_items:
            item.show_basename()

    def on_file_click(self, filename, *args):
        print("FileClick:", filename, args)


class FileFilter(QLineEdit):

    def __init__(self, *args):
        super().__init__(*args)


class FileViewWindow(QMainWindow):

    def __init__(self, *args):
        super().__init__(*args)

        self.setWindowTitle("dt-fileview")
        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = DetailView()
        self.thumb_view = ThumbView()
        self.file_filter = FileFilter()
        self.file_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.vbox.addWidget(self.file_view, Qt.AlignLeft)
        self.thumb_view.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.vbox.addWidget(self.thumb_view, Qt.AlignLeft)
        self.vbox.addWidget(self.file_filter)
        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

        self.toolbar = self.addToolBar("FileView")
        self.toolbar.addAction("Show AbsPath", self.file_view.show_abspath)
        self.toolbar.addAction("Show Basename", self.file_view.show_basename)
        self.toolbar.addAction("Show Time Gaps", self.file_view.toggle_timegaps)
        self.toolbar.addAction("Thumbnail View", self.toggle_thing)
        self.toolbar.addAction("Detail View", self.toggle_thing)
        info = QLabel("lots of files selected")
        self.toolbar.addWidget(info)

        self.file_filter.setFocus()

    def toggle_thing(self):
        print("toggle_thing")


def parse_args(args):
    parser = argparse.ArgumentParser(description="Display files graphically")
    parser.add_argument("FILE", nargs='*')
    parser.add_argument("-t", "--timespace", action='store_true',
                        help="Space items appart given their mtime")
    parser.add_argument("-0", "--null", action='store_true',
                        help="Read \\0 separated lines")
    parser.add_argument("-r", "--recursive", action='store_true',
                        help="Be recursive")
    return parser.parse_args(args)


def expand_file(f, recursive):
    if os.path.isdir(f):
        if recursive:
            lst = [expand_file(os.path.join(f, x), recursive) for x in os.listdir(f)]
            return [item for sublist in lst for item in sublist]
        else:
            return [os.path.join(f, x) for x in os.listdir(f)]
    else:
        return [f]


def expand_directories(files, recursive):
    results = []
    for f in files:
        results += expand_file(f, recursive)
    return results


def get_file_list(args):
    if args.null:
        return filter(bool, sys.stdin.read().split('\0'))
    elif args.FILE == []:
        return sys.stdin.read().splitlines()
    else:
        return args.FILE


def main(argv):
    # Allow Ctrl-C killing of the Qt app, see:
    # http://stackoverflow.com/questions/4938723/
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    args = parse_args(argv[1:])
    files = get_file_list(args)

    app = QApplication([])
    window = FileViewWindow()
    window.file_view.timespace = args.timespace
    files = expand_directories(files, args.recursive)
    window.file_view.add_files(files)
    window.thumb_view.add_files(files)
    window.show()

    rc = app.exec_()

    # All Qt classes need to be destroyed before the app exists or we
    # get a segfault
    del window
    del app

    sys.exit(rc)


def main_entrypoint():
    main(sys.argv)


# EOF #
