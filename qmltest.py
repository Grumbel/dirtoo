#!/usr/bin/env python3

import signal

from PyQt5.QtCore import Qt, QUrl, QByteArray
from PyQt5.QtWidgets import QApplication
from PyQt5.QtQuick import QQuickView, QQuickItem
from PyQt5.QtQml import QQmlComponent


def main():
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    qapp = QApplication([])

    view = QQuickView()
    view.setSource(QUrl('main.qml'))
    # view.setResizeMode(QQuickView.SizeRootObjectToView)
    view.resize(1024, 768)
    view.show()

    engine = view.engine()
    component = QQmlComponent(engine)
    component.setData(b"import QtQuick 2.0\nText { text: \"Hello world!\" }", QUrl())
    item = component.create()
    item.setParentItem(view.rootObject())

    qapp.exec()



if __name__ == "__main__":
    main()


# EOF #
