#!/usr/bin/env python3

import dbus
import xml
import xml.dom.minidom
import xml.etree.ElementTree as ET

bus = dbus.SystemBus()

ud_manager_obj = bus.get_object("org.freedesktop.UDisks2", "/org/freedesktop/UDisks2")

manager_obj = bus.get_object("org.freedesktop.UDisks2", "/org/freedesktop/UDisks2/Manager")
manager_if = dbus.Interface(manager_obj, "org.freedesktop.UDisks2.Manager")
devs = manager_if.GetBlockDevices({})
for dev in devs:
    # fs_if = dbus.Interface(dev, "org.freedesktop.UDisks2.Filesystem")
    obj = bus.get_object("org.freedesktop.UDisks2", dev)
    fs_if = dbus.Interface(obj, dbus.PROPERTIES_IFACE)
    try:
        block_string = fs_if.Get("org.freedesktop.UDisks2.Block", "Device")
        byte_string = fs_if.Get("org.freedesktop.UDisks2.Filesystem", "MountPoints")
        sym_string = fs_if.Get("org.freedesktop.UDisks2.Block", "Symlinks")

        # always 0
        size = fs_if.Get("org.freedesktop.UDisks2.Filesystem", "Size")

        print(bytes(block_string).decode())

        print("\n  Symlinks:")
        for b in sym_string:
            print("   ", bytes(b).decode())

        print("\n  Mountpoints:")
        for b in byte_string:
            print("   ", bytes(b).decode())

        print()
    except Exception as err:
        pass # print(err)

if False:
    ud_manager = dbus.Interface(ud_manager_obj, 'org.freedesktop.DBus.ObjectManager')

    for dev in ud_obj_manager.GetManagedObjects():
        print(dev)
        device_obj = bus.get_object("org.freedesktop.UDisks2", dev)
        device_intro = dbus.Interface(device_obj, "org.freedesktop.DBus.Introspectable")

        xml_text = device_intro.Introspect()
        root = ET.fromstring(xml_text)

        dom = xml.dom.minidom.parseString(xml_text)
        # dom.documentElement.findall("node")

        device_props = dbus.Interface(device_obj, dbus.PROPERTIES_IFACE)

        for el in root.findall("./interface"):
            print("  ", el.attrib['name'])

        if True:
            props = []
            try:
                # props = device_props.GetAll('org.freedesktop.UDisks2.Block')
                props = device_props.GetAll(dbus.PROPERTIES_IFACE)
            except Exception:
                pass

            for prop in props:
                value = device_props.Get('org.freedesktop.UDisks2.Block', prop)
                print("    {:>30s} : {}".format(prop, str(value)[:60]))
            print()

    # print(dbus.PROPERTIES_IFACE)
    # "org.freedesktop.DBus.Properties"

# EOF #
