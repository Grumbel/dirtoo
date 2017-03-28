import os
import sys
import time
import argparse
import datetime
import hashlib
import urllib.parse
import signal
import subprocess
import html

from PyQt5 import QtCore, QtGui
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QPixmap, QColor, QIcon
from PyQt5.QtWidgets import (
    QWidget, QApplication, QGraphicsView,
    QGraphicsScene, QGraphicsItem, QGridLayout, QVBoxLayout,
    QHBoxLayout, QLabel, QLineEdit, QPushButton,
    QGraphicsTextItem, QGraphicsItemGroup, QGraphicsPixmapItem,
    QMainWindow
)


def make_thumbnail_filename(filename):
    url = "file://" + urllib.parse.quote(os.path.abspath(filename))
    digest = hashlib.md5(os.fsencode(url)).hexdigest()
    result = os.path.expanduser(os.path.join("~/.thumbnails/normal", digest + ".png"))
    if os.path.exists(result):
        return result
    else:
        return None


class FileItem(QGraphicsItemGroup):

    def __init__(self, filename):
        super().__init__()
        self.thumbnail = None

        self.filename = filename
        self.text = QGraphicsTextItem(filename)
        self.addToGroup(self.text)

    def mousePressEvent(self, ev):
        print("FileItem:click: ", self.filename)
        subprocess.Popen(["xdg-open", self.filename])

    def hoverEnterEvent(self, ev):
        # print("Enter", ev, self.filename)
        self.show_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 128, 128))

    def hoverLeaveEvent(self, ev):
        # print("Leave", ev, self.filename)
        self.hide_thumbnail()
        self.text.setDefaultTextColor(QColor(0, 0, 0))

    def show_thumbnail(self):
        if self.thumbnail is None:
            thumbnail_filename = make_thumbnail_filename(self.filename)
            if thumbnail_filename is None:
                print("no thumbnail for", self.filename)
            else:
                print("showing thumbnail:", thumbnail_filename)
                self.thumbnail = QGraphicsPixmapItem(QPixmap(thumbnail_filename))
                self.thumbnail.setPos(self.pos())
                self.addToGroup(self.thumbnail)
                self.thumbnail.show()

    def hide_thumbnail(self):
        if self.thumbnail is not None:
            # print("hiding thumbnail", self.filename)
            self.removeFromGroup(self.thumbnail)
            self.thumbnail = None

    def show_abspath(self):
        abspath = os.path.abspath(self.filename)
        dirname = os.path.dirname(abspath)
        basename = os.path.basename(abspath)
        html_text = '<font color="grey">{}/</font>{}'.format(
            html.escape(dirname),
            html.escape(basename))
        self.text.setHtml(html_text)

    def show_basename(self):
         self.text.setPlainText(os.path.basename(self.filename))


class FileView(QGraphicsView):

    def __init__(self):
        super().__init__()

        self.timespace = False

        self.scene = QGraphicsScene()
        # self.scene.addRect(10, 10, 64, 64)
        # self.scene.addText("Hello world")
        # self.scene.setSceneRect(0,0,600,400)

        self.setScene(self.scene)
        self.file_items = []

    def add_files(self, files):
        last_mtime = None

        files = [(os.path.getmtime(f), f) for f in files]
        files = sorted(files)

        y = 10
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
        print("FileClick:", fileclick, args)


class FileFilter(QLineEdit):

    def __init__(self, *args):
        super().__init__(*args)


class FileViewWindow(QMainWindow):

    def __init__(self, *args):
        super().__init__(*args)

        self.vbox = QVBoxLayout()
        self.vbox.setContentsMargins(0, 0, 0, 0)

        self.file_view = FileView()
        self.file_filter = FileFilter()
        self.vbox.addWidget(self.file_view)
        self.vbox.addWidget(self.file_filter)

        vbox_widget = QWidget()
        vbox_widget.setLayout(self.vbox)
        self.setCentralWidget(vbox_widget)

        self.toolbar = self.addToolBar("FileView")
        self.toolbar.addAction("Show AbsPath", self.file_view.show_abspath)
        self.toolbar.addAction("Show Basename", self.file_view.show_basename)
        self.toolbar.addAction("Show Time Gaps", self.file_view.show_basename)
        self.toolbar.addAction("Thumbnail View", self.toggle_thing)
        self.toolbar.addAction("Detail View", self.toggle_thing)

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


def expand_directories(files):
    results = []
    for f in files:
        if os.path.isdir(f):
            results += [os.path.join(f, x) for x in os.listdir(f)]
        else:
            results.append(f)
    return results


def main():
    # Allow Ctrl-C killing of the Qt app, see:
    # http://stackoverflow.com/questions/4938723/
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    args = parse_args(sys.argv[1:])

    if args.null:
        files = filter(bool, sys.stdin.read().split('\0'))
    elif args.FILE == []:
        files = sys.stdin.read().splitlines()
    else:
        files = args.FILE

    app = QApplication([])
    signal.signal(signal.SIGINT,signal.SIG_DFL)
    window = FileViewWindow()
    window.file_view.timespace = args.timespace
    files = expand_directories(files)
    window.file_view.add_files(files)
    window.show()

    sys.exit(app.exec_())


# EOF #
