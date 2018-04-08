#!/usr/bin/env python3

import dbus
import xml
import xml.dom.minidom
import xml.etree.ElementTree as ET

bus = dbus.SystemBus()
ud_manager_obj = bus.get_object(
    "org.freedesktop.UDisks2",
    "/org/freedesktop/UDisks2"
)
ud_manager = dbus.Interface(ud_manager_obj, 'org.freedesktop.DBus.ObjectManager')

for dev in ud_manager.GetManagedObjects():
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
