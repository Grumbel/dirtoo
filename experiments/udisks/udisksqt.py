#!/usr/bin/env python3

import signal

from PyQt5.QtDBus import QDBusConnection
from PyQt5.QtDBus import QDBusReply, QDBusMessage, QDBusInterface
from PyQt5.QtCore import QObject, pyqtSlot, QVariant, QCoreApplication


def call(obj, method, *args):
    msg = obj.call(method, *args)
    reply = QDBusReply(msg)
    if not reply.isValid():
        raise Exception("Error on method call '{}': {}: {}".format(
            method,
            reply.error().name(),
            reply.error().message()))
    else:
        return msg.arguments()


class UDiskManager(QObject):

    def __init__(self, bus):
        super().__init__()

        bus.registerObject('/', self)

        bus.connect('', '',
                    'org.freedesktop.DBus.ObjectManager',
                    'InterfacesAdded', self._on_interfaces_added)

        bus.connect('', '',
                    'org.freedesktop.DBus.ObjectManager',
                    'InterfacesRemoved', self._on_interfaces_removed)

        self.udisks_manager = QDBusInterface("org.freedesktop.UDisks2",
                                             "/org/freedesktop/UDisks2/Manager",
                                             "org.freedesktop.UDisks2.Manager",
                                             connection=bus)
        print("Results: ", call(self.udisks_manager, "GetBlockDevices", {}))

    @pyqtSlot(QDBusMessage)
    def _on_interfaces_added(self, msg):
        print("added", msg.arguments())

    @pyqtSlot(QDBusMessage)
    def _on_interfaces_removed(self, msg):
        print("removed", msg.arguments())


def main():
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    app = QCoreApplication([])
    bus = QDBusConnection.systemBus()

    udisk_manager = UDiskManager(bus)
    app.exec()


if __name__ == "__main__":
    main()


# EOF #
